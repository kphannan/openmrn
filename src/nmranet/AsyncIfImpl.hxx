/** \copyright
 * Copyright (c) 2013, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file AsyncIfImpl.hxx
 *
 * Implementation details for the asynchronous NMRAnet interfaces. This file
 * should only be needed in hardware interface implementations.
 *
 * @author Balazs Racz
 * @date 4 Dec 2013
 */

#ifndef _NMRAnetAsyncIfImpl_hxx_
#define _NMRAnetAsyncIfImpl_hxx_

#include "nmranet/AsyncIf.hxx"

namespace NMRAnet
{

/** Implementation of the hardware-independent parts of the write flows. */
class WriteFlowBase : public StateFlow<Buffer<NMRAnetMessage>, QList<4>>
{
public:
    WriteFlowBase(AsyncIf *async_if)
        : StateFlow<Buffer<NMRAnetMessage>, QList<4>>(async_if)
    {
    }

protected:
    /** This function will be called (on the main executor) to initiate sending
     * this message to the hardware. The flow will then execute the returned
     * action.
     *
     * NOTE: it is possible that this functon will never be called for a given
     * flow. */
    virtual Action send_to_hardware() = 0;

    /** Virtual method called after the send is completed, i.e., all the frames
     * are generated and sent to the hardware. Various flows might need to take
     * additional steps afterwards. */
    virtual Action send_finished()
    {
        return release_and_exit();
    }

    /// @returns the interface that this flow is assigned to.
    AsyncIf *async_if()
    {
        return static_cast<AsyncIf *>(service());
    }

    /** Implementations shall call this function when they are done with
     * sending the packet.
     */
    // void cleanup();

    /// Returns the NMRAnet message we are trying to send.
    NMRAnetMessage *nmsg()
    {
        return message()->data();
    }

protected:
    /*    /// Entry point for external callers.
        virtual void WriteAddressedMessage(If::MTI mti, NodeID src, NodeHandle
       dst,
                                           Buffer* data, Notifiable* done)
        {
            HASSERT(IsNotStarted());
            Restart(done);
            mti_ = mti;
            src_ = src;
            dst_ = dst;
            dstNode_ = nullptr;
            data_ = data;
            StartFlowAt(STATE(maybe_send_to_local_node));
        }

        /// Entry point for external callers.
        virtual void WriteGlobalMessage(If::MTI mti, NodeID src, Buffer* data,
                                        Notifiable* done)
        {
            HASSERT(IsNotStarted());
            Restart(done);
            mti_ = mti;
            src_ = src;
            dst_.id = 0;
            dst_.alias = 0;
            dstNode_ = nullptr;
            data_ = data;
            StartFlowAt(STATE(send_to_local_nodes));
        }
    */

    /** Addressed write flows should call this state BEFORE sending to the
     * hardware. They may get back control at the send_to_hardware state if
     * needed.
     * NOTE: datagram write flow cannot use this because it won't get back. */
    Action addressed_entry();
    /** Global write flows should return to this state AFTER sending the
     * message to the hardware. They should ensure the message is still
     * intact. They will not get back control. */
    Action global_entry();
};

/** This handler handles VerifyNodeId messages (both addressed and global) on
 * the interface level. Each interface implementation will want to create one
 * of these. */
class VerifyNodeIdHandler : public IncomingMessageStateFlow
{
public:
    VerifyNodeIdHandler(AsyncIf *service) : IncomingMessageStateFlow(service)
    {
        interface()->dispatcher()->register_handler(
            this,
            If::MTI_VERIFY_NODE_ID_GLOBAL & If::MTI_VERIFY_NODE_ID_ADDRESSED,
            0xffff & ~(If::MTI_VERIFY_NODE_ID_GLOBAL ^
                       If::MTI_VERIFY_NODE_ID_ADDRESSED));
    }

    ~VerifyNodeIdHandler()
    {
        interface()->dispatcher()->unregister_handler(
            this,
            If::MTI_VERIFY_NODE_ID_GLOBAL & If::MTI_VERIFY_NODE_ID_ADDRESSED,
            0xffff & ~(If::MTI_VERIFY_NODE_ID_GLOBAL ^
                       If::MTI_VERIFY_NODE_ID_ADDRESSED));
    }

    /// Handler callback for incoming messages.
    virtual Action entry()
    {
        NMRAnetMessage *m = message()->data();
        if (m->dst.id)
        {
            // Addressed message.
            srcNode_ = m->dstNode;
        }
        else if (!m->payload.empty() && m->payload.size() == 6)
        {
            // Global message with a node id included
            NodeID id = buffer_to_node_id(m->payload);
            srcNode_ = interface()->lookup_local_node(id);
        }
        else
        {
// Global message. Everyone should respond.
#ifdef SIMPLE_NODE_ONLY
            // We assume there can be only one local node.
            AsyncIf::VNodeMap::Iterator it = interface()->localNodes_.begin();
            if (it == interface()->localNodes_.end())
            {
                // No local nodes.
                return release_and_exit();
            }
            srcNode_ = it->second;
            ++it;
            HASSERT(it == interface()->localNodes_.end());
#else
            // We need to do an iteration over all local nodes.
            it_ = interface()->localNodes_.begin();
            if (it_ == interface()->localNodes_.end())
            {
                // No local nodes.
                return release_and_exit();
            }
            srcNode_ = it_->second;
            ++it_;
#endif // not simple node.
        }
        if (srcNode_)
        {
            release();
            return allocate_and_call(interface()->global_message_write_flow(),
                                     STATE(send_response));
        }
        else
        {
            LOG(WARNING, "node pointer not found.");
            return release_and_exit();
        }
    }

#ifdef SIMPLE_NODE_ONLY
    Action send_response()
    {
        auto *b =
            get_allocation_result(interface()->global_message_write_flow());
        NMRAnetMessage *m = b->data();
        NodeID id = srcNode_->node_id();
        m->reset(If::MTI_VERIFIED_NODE_ID_NUMBER, id, node_id_to_buffer(id));
        interface_->global_message_write_flow()->send(b);
    }
#else
    Action send_response()
    {
        auto *b =
            get_allocation_result(interface()->global_message_write_flow());
        NMRAnetMessage *m = b->data();
        // This is called on the main executor of the interface, this we are
        // allowed to access the local nodes cache.
        NodeID id = srcNode_->node_id();
        LOG(VERBOSE, "Sending verified reply from node %012llx", id);
        m->reset(If::MTI_VERIFIED_NODE_ID_NUMBER, id, node_id_to_buffer(id));
        interface()->global_message_write_flow()->send(b);

        /** Continues the iteration over the nodes.
         *
         * @TODO(balazs.racz): we should probably wait for the outgoing message
         * to be sent. */
        if (it_ != interface()->localNodes_.end())
        {
            srcNode_ = it_->second;
            ++it_;
            return allocate_and_call(interface()->global_message_write_flow(),
                                     STATE(send_response));
        }
        else
        {
            return exit();
        }
    }
#endif // not simple node

private:
    AsyncNode *srcNode_;

#ifndef SIMPLE_NODE_ONLY
    AsyncIf::VNodeMap::Iterator it_;
#endif
};
} // namespace NMRAnet

#endif // _NMRAnetAsyncIfImpl_hxx_
