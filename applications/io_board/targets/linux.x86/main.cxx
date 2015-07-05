/** \copyright
 * Copyright (c) 2013, Balazs Racz
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
 * \file main.cxx
 *
 * Main file for the io board application on the Tiva Launchpad board.
 *
 * @author Balazs Racz
 * @date 5 Jun 2015
 */

#include "os/os.h"
#include "nmranet_config.h"

#include "nmranet/SimpleStack.hxx"
#include "nmranet/ConfiguredConsumer.hxx"
#include "nmranet/ConfiguredProducer.hxx"

#include "config.hxx"

// Changes the default behavior by adding a newline after each gridconnect
// packet. Makes it easier for debugging the raw device.
OVERRIDE_CONST(gc_generate_newlines, 1);
// Specifies how much RAM (in bytes) we allocate to the stack of the main
// thread. Useful tuning parameter in case the application runs out of memory.
OVERRIDE_CONST(main_thread_stack_size, 2500);

// Specifies the 48-bit OpenLCB node identifier. This must be unique for every
// hardware manufactured, so in production this should be replaced by some
// easily incrementable method.
extern const nmranet::NodeID NODE_ID = 0x050101011409ULL;

// Sets up a comprehensive OpenLCB stack for a single virtual node. This stack
// contains everything needed for a usual peripheral node -- all
// CAN-bus-specific components, a virtual node, PIP, SNIP, Memory configuration
// protocol, ACDI, CDI, a bunch of memory spaces, etc.
nmranet::SimpleCanStack stack(NODE_ID);

// ConfigDef comes from config.hxx and is specific to the particular device and
// target. It defines the layout of the configuration memory space and is also
// used to generate the cdi.xml file. Here we instantiate the configuration
// layout. The argument of offset zero is ignored and will be removed later.
nmranet::ConfigDef cfg(0);
// Defines weak constants used by the stack to tell it which device contains
// the volatile configuration information. This device name appears in
// HwInit.cxx that creates the device drivers.
extern const char *const nmranet::CONFIG_FILENAME = "/tmp/config_eeprom";
// The size of the memory space to export over the above device.
extern const size_t nmranet::CONFIG_FILE_SIZE =
    cfg.seg().size() + cfg.seg().offset();
static_assert(nmranet::CONFIG_FILE_SIZE <= 256, "Need to adjust eeprom size");
// The SNIP user-changeable information in also stored in the above eeprom
// device. In general this could come from different eeprom segments, but it is
// simpler to keep them together.
extern const char *const nmranet::SNIP_DYNAMIC_FILENAME =
    nmranet::CONFIG_FILENAME;

// Defines the GPIO ports used for the producers and the consumers.

struct DummyPin {
    static bool get() { return false;}
    static void set(bool v) {}
};

// The first LED is driven by the blinker device from BlinkerGPIO.hxx. We just
// create an alias for symmetry.
typedef DummyPin LED_RED_Pin;
typedef DummyPin LED_GREEN_Pin;
typedef DummyPin LED_BLUE_Pin;
typedef DummyPin SW1_Pin;
typedef DummyPin SW2_Pin;

// Instantiates the actual producer and consumer objects for the given GPIO
// pins from above. The ConfiguredConsumer class takes care of most of the
// complicated setup and operation requirements. We need to give it the virtual
// node pointer, the configuration configuration from the CDI definition, and
// the hardware pin definition. The virtual node pointer comes from the stack
// object. The configuration structure comes from the CDI definition object,
// segment 'seg', in which there is a repeated group 'consumers', and we assign
// the individual entries to the individual consumers. Each consumer gets its
// own GPIO pin.
nmranet::ConfiguredConsumer consumer_red(
    stack.node(), cfg.seg().consumers().entry<0>(), LED_RED_Pin());
nmranet::ConfiguredConsumer consumer_green(
    stack.node(), cfg.seg().consumers().entry<1>(), LED_GREEN_Pin());
nmranet::ConfiguredConsumer consumer_blue(
    stack.node(), cfg.seg().consumers().entry<2>(), LED_BLUE_Pin());

// Similar syntax for the producers.
nmranet::ConfiguredProducer producer_sw1(
    stack.node(), cfg.seg().producers().entry<0>(), SW1_Pin());
nmranet::ConfiguredProducer producer_sw2(
    stack.node(), cfg.seg().producers().entry<1>(), SW2_Pin());

// The producers need to be polled repeatedly for changes and to execute the
// debouncing algorithm. This class instantiates a refreshloop and adds the two
// producers to it.
nmranet::RefreshLoop loop(
    stack.node(), {producer_sw1.polling(), producer_sw2.polling()});

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char *argv[])
{
    // Connects to a TCP hub on the internet.
    stack.connect_tcp_gridconnect_hub("28k.ch", 50007);
    // Causes all packets to be dumped to stdout.
    stack.print_all_packets();
    // This command donates the main thread to the operation of the
    // stack. Alternatively the stack could be started in a separate stack and
    // then application-specific business logic could be executed ion a busy
    // loop in the main thread.
    stack.loop_executor();
    return 0;
}