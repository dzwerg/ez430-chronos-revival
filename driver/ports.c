// *************************************************************************************************
//
//	Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/ 
//	 
//	 
//	  Redistribution and use in source and binary forms, with or without 
//	  modification, are permitted provided that the following conditions 
//	  are met:
//	
//	    Redistributions of source code must retain the above copyright 
//	    notice, this list of conditions and the following disclaimer.
//	 
//	    Redistributions in binary form must reproduce the above copyright
//	    notice, this list of conditions and the following disclaimer in the 
//	    documentation and/or other materials provided with the   
//	    distribution.
//	 
//	    Neither the name of Texas Instruments Incorporated nor the names of
//	    its contributors may be used to endorse or promote products derived
//	    from this software without specific prior written permission.
//	
//	  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//	  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//	  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//	  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//	  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//	  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//	  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//	  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//	  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//	  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//	  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *************************************************************************************************
// Button entry functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"

// driver
#include "ports.h"
#include "buzzer.h"
#include "vti_ps.h"
#include "timer.h"
#include "display.h"

// logic
#include "clock.h"
#include "alarm.h"
#include "altitude.h"
#include "stopwatch.h"


// *************************************************************************************************
// Prototypes section
void button_repeat_on(u16 msec);
void button_repeat_off(void);
void button_repeat_function(void);


// *************************************************************************************************
// Defines section

// Macro for button IRQ 
#define IRQ_TRIGGERED(flags, bit)		((flags & bit) == bit)


// *************************************************************************************************
// Global Variable section
volatile s_button_flags button;
volatile struct struct_button sButton;


// *************************************************************************************************
// Extern section
extern void (*fptr_Timer0_A3_function)(void);


// *************************************************************************************************
// @fn          init_buttons
// @brief       Init and enable button interrupts.
// @param       none
// @return      none
// *************************************************************************************************
void init_buttons(void)
{
	// Set button ports to input 
	BUTTONS_DIR &= ~ALL_BUTTONS; 

	// Enable internal pull-downs
	BUTTONS_OUT &= ~ALL_BUTTONS; 
	BUTTONS_REN |= ALL_BUTTONS; 

	// IRQ triggers on rising edge
	BUTTONS_IES &= ~ALL_BUTTONS;   

	// Reset IRQ flags
	BUTTONS_IFG &= ~ALL_BUTTONS;  

	// Enable button interrupts
	BUTTONS_IE |= ALL_BUTTONS;   
}




// *************************************************************************************************
// @fn          PORT2_ISR
// @brief       Interrupt service routine for
//					- buttons 
//					- pressure sensor DRDY
// @param       none
// @return      none
// *************************************************************************************************
__attribute__((interrupt(PORT2_VECTOR))) void PORT2_ISR(void)
{
    u8 int_flag = BUTTONS_IFG & BUTTONS_IE;
    extern volatile u8 g_set_mode_active;
    extern volatile u8 g_alarm_up_irq;
    static u8 set_up_latched = 0;
    static u8 set_down_latched = 0;

    BUTTONS_IFG &= (u8)~int_flag;

    /* The pressure sensor still shares PORT2 with the buttons. */
    if (int_flag & PS_INT_PIN)
        request.flag.altitude_measurement = 1;

    /* fixed55: UP uses the proven PORT2 IRQ path for the alarm menu too. */
    if ((int_flag & BUTTON_UP_PIN) && BUTTON_UP_IS_PRESSED)
        g_alarm_up_irq = 1;

    /* Outside the blocking set menus, the ISR is only a low-power wake source.
       The foreground state machine performs the real 20-ms debounce and
       short/long-press recognition. */
    if (!g_set_mode_active && (set_up_latched || set_down_latched))
    {
        set_up_latched = 0;
        set_down_latched = 0;
        BUTTONS_IES &= (u8)~(BUTTON_UP_PIN | BUTTON_DOWN_PIN);
        BUTTONS_IFG &= (u8)~(BUTTON_UP_PIN | BUTTON_DOWN_PIN);
    }

    if (g_set_mode_active)
    {
        if ((int_flag & BUTTON_STAR_PIN) && BUTTON_STAR_IS_PRESSED)
            button.flag.star = 1;
        else if ((int_flag & BUTTON_NUM_PIN) && BUTTON_NUM_IS_PRESSED)
            button.flag.num = 1;
        else if (int_flag & BUTTON_UP_PIN)
        {
            /* fixed69: one physical UP press = one set-value event.
               After accepting the press, switch P2.4 to the release edge.
               Contact bounce can no longer generate extra increments. */
            if (!set_up_latched && BUTTON_UP_IS_PRESSED)
            {
                set_up_latched = 1;
                button.flag.up = 1;
                BUTTONS_IES |= BUTTON_UP_PIN;   /* high->low: wait for release */
            }
            else if (set_up_latched && BUTTON_UP_IS_RELEASED)
            {
                set_up_latched = 0;
                BUTTONS_IFG &= (u8)~BUTTON_UP_PIN;
                BUTTONS_IES &= (u8)~BUTTON_UP_PIN; /* low->high: next press */
            }
        }
        else if (int_flag & BUTTON_DOWN_PIN)
        {
            /* fixed70: DOWN uses the same one-press/one-event latch as UP. */
            if (!set_down_latched && BUTTON_DOWN_IS_PRESSED)
            {
                set_down_latched = 1;
                button.flag.down = 1;
                BUTTONS_IES |= BUTTON_DOWN_PIN;   /* high->low: wait release */
            }
            else if (set_down_latched && BUTTON_DOWN_IS_RELEASED)
            {
                set_down_latched = 0;
                BUTTONS_IFG &= (u8)~BUTTON_DOWN_PIN;
                BUTTONS_IES &= (u8)~BUTTON_DOWN_PIN; /* low->high: next press */
            }
        }
        else if ((int_flag & BUTTON_BACKLIGHT_PIN) && BUTTON_BACKLIGHT_IS_PRESSED)
            button.flag.backlight = 1;
    }

    _BIC_SR_IRQ(LPM3_bits);
}


// *************************************************************************************************
// @fn          button_repeat_on
// @brief       Start button auto repeat timer.
// @param       none
// @return      none
// *************************************************************************************************
void button_repeat_on(u16 msec)
{
	// Set button repeat flag
	sys.flag.up_down_repeat_enabled = 1;
	
	// Set Timer0_A3 function pointer to button repeat function
	fptr_Timer0_A3_function = button_repeat_function;
	
	// Timer0_A3 IRQ triggers every 200ms
	Timer0_A3_Start(CONV_MS_TO_TICKS(msec));
}


// *************************************************************************************************
// @fn          button_repeat_off
// @brief       Stop button auto repeat timer.
// @param       none
// @return      none
// *************************************************************************************************
void button_repeat_off(void)
{
	// Clear button repeat flag
	sys.flag.up_down_repeat_enabled = 0;
	
	// Timer0_A3 IRQ repeats with 4Hz
	Timer0_A3_Stop();
}


// *************************************************************************************************
// @fn          button_repeat_function
// @brief       Check at regular intervals if button is pushed continuously 
//				and trigger virtual button event.
// @param       none
// @return      none
// *************************************************************************************************
void button_repeat_function(void)
{
	static u8 start_delay = 10;	// Wait for 2 seconds before starting auto up/down
	u8 repeat = 0;
	
	// If buttons UP or DOWN are continuously high, repeatedly set button flag
	if (BUTTON_UP_IS_PRESSED)
	{
		if (start_delay == 0)
		{
			// Generate a virtual button event
			button.flag.up = 1;
			repeat = 1;
		}
		else
		{
			start_delay--;
		}
	}
	else if (BUTTON_DOWN_IS_PRESSED)
	{
		if (start_delay == 0)
		{
			// Generate a virtual button event
			button.flag.down = 1;
			repeat = 1;
		}
		else
		{
			start_delay--;
		}
	}
	else
	{
		// Reset repeat counter
		sButton.repeats = 0;
		start_delay = 10;

		// Enable blinking
		start_blink();
	}
	
	// If virtual button event is generated, stop blinking and reset timeout counter
	if (repeat)
	{
		// Increase repeat counter
		sButton.repeats++;

		// Reset inactivity detection counter
		sTime.last_activity = sTime.system_time;
		
		// Disable blinking
		stop_blink();
	}
}
