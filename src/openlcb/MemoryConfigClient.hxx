/** \copyright
 * Copyright (c) 2017, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
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
 * \file MemoryConfigClient.hxx
 *
 * Utility flow to talk to a remote device using the memconfig protocol.
 *
 * @author Balazs Racz
 * @date 4 Feb 2017
 */

#ifndef _OPENLCB_MEMORYCONFIGCLIENT_HXX_
#define _OPENLCB_MEMORYCONFIGCLIENT_HXX_

#include "executor/CallableFlow.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "openlcb/DatagramHandlerDefault.hxx"

namespace openlcb
{

struct MemoryConfigClientRequest : public CallableFlowRequestBase
{
    enum ReadCmd
    {
        READ
    };

    /// Sets up a command to read an entire memory space.
    /// @param ReadCmd polymorphic matching arg; always set to READ.
    /// @param d is the destination node to query
    /// @param space is the memory space to read out
    void reset(ReadCmd, NodeHandle d, uint8_t space)
    {
        reset_base();
        cmd = CMD_READ;
        memory_space = space;
        dst = d;
        payload.clear();
    }

    enum Command : uint8_t
    {
        CMD_READ,
        CMD_WRITE
    };
    Command cmd;
    uint8_t memory_space;
    /// Node to send the request to.
    NodeHandle dst;
    string payload;
};

class MemoryConfigClient : public CallableFlow<MemoryConfigClientRequest>
{
public:
    MemoryConfigClient(Node *node, MemoryConfigHandler *memcfg)
        : CallableFlow<MemoryConfigClientRequest>(memcfg->dg_service())
        , node_(node)
        , memoryConfigHandler_(memcfg)
    {
    }

private:
    Action entry() override
    {
        switch (request()->cmd)
        {
        case MemoryConfigClientRequest::CMD_READ:
                return allocate_and_call(
                    STATE(do_read), dg_service()->client_allocator());
        default: break;
        }
        return return_with_error(Defs::ERROR_UNIMPLEMENTED_SUBCMD);
    }

    Action do_read()
    {
        dgClient_ = full_allocation_result(dg_service()->client_allocator());
        offset_ = 0;
        memoryConfigHandler_->set_client(&responseFlow_);
        return call_immediately(STATE(send_next_read));
    }

    Action send_next_read()
    {
        return allocate_and_call(
            dg_service()->iface()->dispatcher(), STATE(send_read_datagram));
    }

    Action send_read_datagram()
    {
        auto *b = get_allocation_result(dg_service()->iface()->dispatcher());
        b->set_done(bn_.reset(this));
        b->data()->reset(Defs::MTI_DATAGRAM, node_->node_id(), request()->dst,
            MemoryConfigDefs::read_datagram(
                             request()->memory_space, offset_, 64));
        isWaitingForTimer_ = 0;
        responseCode_ = DatagramClient::OPERATION_PENDING;
        dgClient_->write_datagram(b);
        return wait_and_call(STATE(read_complete));
    }

    Action read_complete()
    {
        if (!(dgClient_->result() & DatagramClient::OPERATION_SUCCESS))
        {
            // some error occurred.
            return handle_read_error(dgClient_->result());
        }
        if (responseCode_ & DatagramClient::OPERATION_PENDING)
        {
            isWaitingForTimer_ = 1;
            return sleep_and_call(
                &timer_, SEC_TO_NSEC(3), STATE(read_response_timeout));
        }
        else
        {
            return call_immediately(STATE(read_response_timeout));
        }
    }

    Action read_response_timeout()
    {
        if (responseCode_ & DatagramClient::OPERATION_PENDING)
        {
            return handle_read_error(Defs::OPENMRN_TIMEOUT);
        }
        size_t len = responsePayload_.size();
        uint8_t *bytes = (uint8_t *)responsePayload_.data();
        if (len < 6)
        {
            LOG(INFO, "Memory Config client: response datagram payload not "
                      "long enough");
            return handle_read_error(
                Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
        }
        unsigned ofs;
        unsigned a = bytes[2];
        a <<= 8;
        a |= bytes[3];
        a <<= 8;
        a |= bytes[4];
        a <<= 8;
        a |= bytes[5];
        uint8_t space = 0;
        uint8_t cmd = bytes[1];
        if (a != offset_)
        {
            return handle_read_error(Defs::ERROR_OUT_OF_ORDER);
        }
        if (cmd & 3)
        {
            ofs = 6;
            space = 0xFC | (cmd & 3);
        }
        else
        {
            ofs = 7;
            if (len < 7)
            {
                return handle_read_error(
                    Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
            }
            space = bytes[6];
        }
        if (space != request()->memory_space)
        {
            return handle_read_error(Defs::ERROR_OUT_OF_ORDER);
        }
        if ((cmd & ~3) == MemoryConfigDefs::COMMAND_READ_FAILED)
        {
            if (len < ofs + 2)
            {
                return handle_read_error(
                    Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
            }
            uint16_t error = bytes[ofs++];
            error <<= 8;
            error |= bytes[ofs];
            return handle_read_error(error);
        }
        if ((cmd & ~3) != MemoryConfigDefs::COMMAND_READ_REPLY)
        {
            return handle_read_error(Defs::ERROR_UNIMPLEMENTED);
        }
        unsigned dlen = len - ofs;
        request()->payload.append((char *)(bytes + ofs), dlen);
        offset_ += dlen;
        if (dlen < 64)
        {
            return call_immediately(STATE(finish_read));
        }
        return call_immediately(STATE(send_next_read));
    }

    Action handle_read_error(int error) {
        if (error == MemoryConfigDefs::ERROR_OUT_OF_BOUNDS) {
            return finish_read();
        }
        cleanup_read();
        return return_with_error(error);
    }

    void cleanup_read() {
        responsePayload_.clear();
        dg_service()->client_allocator()->typed_insert(dgClient_);
        memoryConfigHandler_->clear_client(&responseFlow_);
        dgClient_ = nullptr;
    }

    Action finish_read() {
        cleanup_read();
        return return_ok();
    }
    
    class ResponseFlow : public DefaultDatagramHandler
    {
    public:
        ResponseFlow(MemoryConfigClient *parent)
            : DefaultDatagramHandler(parent->memoryConfigHandler_->dg_service())
            , parent_(parent)
        {
        }

    private:
        Action entry() override
        {
            if (!parent_->node_->iface()->matching_node(
                    parent_->request()->dst, message()->data()->src))
            {
                return respond_reject(Defs::ERROR_OUT_OF_ORDER);
            }
            if (size() < 2)
            {
                return respond_reject(
                    Defs::ERROR_INVALID_ARGS_MESSAGE_TOO_SHORT);
            }
            auto* bytes = payload();
            uint8_t cmd = bytes[1] & ~3;
            switch (cmd)
            {
                case MemoryConfigDefs::COMMAND_READ_REPLY:
                case MemoryConfigDefs::COMMAND_READ_FAILED:
                {
                    parent_->responseCode_ = 0;
                    message()->data()->payload.swap(parent_->responsePayload_);
                    if (parent_->isWaitingForTimer_)
                    {
                        parent_->timer_.trigger();
                    }
                    return respond_ok(0);
                }
            }
            return respond_reject(Defs::ERROR_UNIMPLEMENTED_SUBCMD);
        }
    private:
        MemoryConfigClient *parent_;        
    };

    DatagramService *dg_service()
    {
        return static_cast<DatagramService *>(service());
    }

    /// Node from which to send the requests out.
    Node *node_;
    /// Hook into the parent node's memory config handler service.
    MemoryConfigHandler *memoryConfigHandler_;
    /// The allocated datagram client which we hold on for the time that we are
    /// querying.
    DatagramClient *dgClient_{nullptr};
    /// Handler for the incoming reply datagrams.
    ResponseFlow responseFlow_{this};
    /// Notify helper.
    BarrierNotifiable bn_;
    /// Next byte to read from the memory space.
    size_t offset_;
    /// timing helper
    StateFlowTimer timer_{this};
    /// The data that came back from reading.
    string responsePayload_;
    /// error code that came with the response. 0 for success.
    int responseCode_;
    /// 1 if we are pending on the timer.
    uint8_t isWaitingForTimer_ : 1;
};

} // namespace openlcb

#endif
