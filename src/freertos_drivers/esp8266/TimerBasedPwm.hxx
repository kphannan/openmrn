#ifndef _DRIVERS_ESP8266_TIMERBASEDPWM_HXX_
#define _DRIVERS_ESP8266_TIMERBASEDPWM_HXX_

extern "C" {
#include <eagle_soc.h>
#include <gpio.h>
}

#include "utils/blinker.h"

//Register definitions

#define FRC1_LOAD READ_PERI_REG(PERIPHS_TIMER_BASEDDR + FRC1_LOAD_ADDRESS)
#define FRC1_COUNT READ_PERI_REG(PERIPHS_TIMER_BASEDDR + FRC1_COUNT_ADDRESS)
#define FRC1_CTRL READ_PERI_REG(PERIPHS_TIMER_BASEDDR + FRC1_CTRL_ADDRESS)
#define FRC1_INTCLR READ_PERI_REG(PERIPHS_TIMER_BASEDDR + FRC1_INT_ADDRESS)

#define FRC_CTL_INT_ACTIVE 0x100

#define FRC_CTL_ENABLE 0x80
#define FRC_CTL_AUTORELOAD 0x40
#define FRC_CTL_DIV_1 0x00
#define FRC_CTL_DIV_16 0x04
#define FRC_CTL_DIV_256 0x0C
#define FRC_CTL_INT_EDGE 0x00
#define FRC_CTL_INT_LEVEL 0x01

#define FRC2_LOAD READ_PERI_REG(PERIPHS_TIMER_BASEDDR + 0x20)
#define FRC2_COUNT READ_PERI_REG(PERIPHS_TIMER_BASEDDR + 0x24)
#define FRC2_CTRL READ_PERI_REG(PERIPHS_TIMER_BASEDDR + 0x28)
#define FRC2_INTCLR READ_PERI_REG(PERIPHS_TIMER_BASEDDR + 0x2C)
#define FRC2_ALARM READ_PERI_REG(PERIPHS_TIMER_BASEDDR + 0x30)


#define GPIO_OUT_CLR GPIO_REG_READ(GPIO_OUT_W1TC_ADDRESS)
#define GPIO_OUT_SET GPIO_REG_READ(GPIO_OUT_W1TS_ADDRESS)


void isr_test() {
    FRC1_INTCLR = 0;
}


class TimerBasedPwm {
public:
    TimerBasedPwm() {
    }

    void __attribute__((noinline)) enable() {
        ETS_FRC1_INTR_DISABLE();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
        ETS_FRC_TIMER1_INTR_ATTACH(&TimerBasedPwm::owned_isr_handler, this);
        //ETS_FRC_TIMER1_NMI_INTR_ATTACH(reinterpret_cast<uint32_t>(&TimerBasedPwm::isr_handler));
#pragma GCC diagnostic pop
    }

    static void ICACHE_RAM_ATTR isr_handler(void*) {
        FRC1_INTCLR = 0;
    }

    void ICACHE_RAM_ATTR owned_isr_handler() {
        if (isOn_) {
            isOn_ = false;
            FRC1_LOAD = clockOff_;
            GPIO_OUT_CLR = gpioValue_;
        } else {
            isOn_ = true;
            FRC1_LOAD = clockOn_;
            GPIO_OUT_SET = gpioValue_;
        }
    }

    /**
     *  Turns off the previous output that may have been running. Call
     *  set_state to revert.
     */
    void pause() {
        ETS_FRC1_INTR_DISABLE();
        if (gpioValue_) {
            GPIO_OUT_CLR = gpioValue_;
        }
    }

    /** Enables the PWM on a specific pin.
     *
     * @param pin is the GPIO number of the pin (0..15)
     * @param nsec_period is the total period length of the output
     * @param nsec_on is the time for which the output shall be turned on
     */
    void old_set_state(int pin, long long nsec_period, long long nsec_on) {
        enable();
        ETS_FRC1_INTR_DISABLE();
        gpioValue_ = 1<<pin;
        if (nsec_on <= 0) {
            GPIO_OUTPUT_SET(pin, 0);
            return;
        }
        if (nsec_on >= nsec_period) {
            GPIO_OUTPUT_SET(pin, 1);
            return;
        }
        // CPU clock = 80 MHz, 12.5 nsec per clock.
        clockOn_ = (nsec_on * 2) / 25;
        long long nsec_off = nsec_period - nsec_on;
        clockOff_ = (nsec_off * 2) / 25;
        HASSERT(clockOn_ < (1<<23));
        HASSERT(clockOff_ < (1<<23));
        GPIO_OUTPUT_SET(pin, 0);
        isOn_ = false;
        FRC1_CTRL = FRC_CTL_ENABLE | FRC_CTL_DIV_1 | FRC_CTL_INT_EDGE;
        FRC1_INTCLR = 0;
        ETS_FRC1_INTR_ENABLE();
        TM1_EDGE_INT_ENABLE();
        FRC1_LOAD = 100; // will trigger interrupt immediately
        TM1_EDGE_INT_ENABLE();
    }

    static void ICACHE_RAM_ATTR new_isr_handler(void*) {
        //if ((T1C & ((1 << TCAR) | (1 << TCIT))) == 0) TEIE &= ~TEIE1;//edge int disable
        FRC1_INTCLR = 0;
        if (isOn_) {
            GPIO_OUT_CLR = gpioValue_;
            isOn_ = false;
            FRC1_LOAD = clockOff_;
            //TM1_EDGE_INT_ENABLE();
        } else {
            GPIO_OUT_SET = gpioValue_;
            isOn_ = true;
            FRC1_LOAD = clockOn_;
            //TM1_EDGE_INT_ENABLE();
        }
    }
    

    void set_state(int pin, long long nsec_period, long long nsec_on) {
        gpioValue_ = 1<<pin;

        // CPU clock = 80 MHz, 12.5 nsec per clock.
        clockOn_ = (nsec_on * 2) / 25;
        long long nsec_off = nsec_period - nsec_on;
        clockOff_ = (nsec_off * 2) / 25;
        HASSERT(clockOn_ < (1<<23));
        HASSERT(clockOff_ < (1<<23));

	//timer1_disable();
        ETS_FRC_TIMER1_INTR_ATTACH(&new_isr_handler, NULL);
        ETS_FRC1_INTR_ENABLE();
	//timer1_attachInterrupt(&TimerBasedPwm::new_isr_handler);
        FRC1_CTRL = FRC_CTL_ENABLE | FRC_CTL_DIV_1 | FRC_CTL_INT_EDGE;
	//timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
        FRC1_LOAD = 1;
        TM1_EDGE_INT_ENABLE();
	//timer1_write(1);
    }

private:
    static uint32_t gpioValue_;
    static uint32_t clockOn_;
    static uint32_t clockOff_;
    static bool isOn_;  // 1 if output is on, 0 if it is off.
};


uint32_t TimerBasedPwm::gpioValue_ = 0;
uint32_t TimerBasedPwm::clockOn_;
uint32_t TimerBasedPwm::clockOff_;
bool TimerBasedPwm::isOn_;


#endif // _DRIVERS_ESP8266_TIMERBASEDPWM_HXX_