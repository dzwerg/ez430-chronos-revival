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
// Several user functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"

// driver
#include "display.h"
#include "buzzer.h"
#include "ports.h"

// logic
#include "menu.h"
#include "date.h"
#include "clock.h"
#include "user.h"
#include "stopwatch.h"


// *************************************************************************************************
// Prototypes section


// *************************************************************************************************
// Defines section


// *************************************************************************************************
// Global Variable section


// *************************************************************************************************
// Extern section
extern void idle_loop(void);


// *************************************************************************************************
// @fn          dummy
// @brief       Dummy direct function.
// @param       u8 line	LINE1, LINE2
// @return      none
// *************************************************************************************************
void dummy(u8 line)
{
}

 
// *************************************************************************************************
// @fn          set_edit_field_blink
// @brief       Copy the currently visible bits of a set_value field into LCD blink memory.
//              This keeps all non-selected fields visible while only the active field blinks.
// @param       segments    One of the LCD_SEG_Lx_y_z field groups used by display_chars().
// @return      none
// *************************************************************************************************
static void set_edit_field_blink(u8 segments)
{
    u8 i;
    u8 length = 0;
    u8 char_start = 0;
    u8 *lcdmem;
    u8 bitmask;

    switch (segments)
    {
        // LINE1
        case LCD_SEG_L1_3_0: length = 4; char_start = LCD_SEG_L1_3; break;
        case LCD_SEG_L1_2_0: length = 3; char_start = LCD_SEG_L1_2; break;
        case LCD_SEG_L1_1_0: length = 2; char_start = LCD_SEG_L1_1; break;
        case LCD_SEG_L1_3_1: length = 3; char_start = LCD_SEG_L1_3; break;
        case LCD_SEG_L1_3_2: length = 2; char_start = LCD_SEG_L1_3; break;

        // LINE2
        case LCD_SEG_L2_5_0: length = 6; char_start = LCD_SEG_L2_5; break;
        case LCD_SEG_L2_4_0: length = 5; char_start = LCD_SEG_L2_4; break;
        case LCD_SEG_L2_3_0: length = 4; char_start = LCD_SEG_L2_3; break;
        case LCD_SEG_L2_2_0: length = 3; char_start = LCD_SEG_L2_2; break;
        case LCD_SEG_L2_1_0: length = 2; char_start = LCD_SEG_L2_1; break;
        case LCD_SEG_L2_5_4: length = 2; char_start = LCD_SEG_L2_5; break;
        case LCD_SEG_L2_5_2: length = 4; char_start = LCD_SEG_L2_5; break;
        case LCD_SEG_L2_3_2: length = 2; char_start = LCD_SEG_L2_3; break;
        case LCD_SEG_L2_4_2: length = 3; char_start = LCD_SEG_L2_4; break;
        default: return;
    }

    for (i = 0; i < length; i++)
    {
        lcdmem = (u8 *)segments_lcdmem[char_start + i];
        bitmask = segments_bitmask[char_start + i];

        /* Blink memory contains the same glyph as normal LCD memory for the
           selected field. All other blink-memory bits were cleared on entry. */
        *(lcdmem + 0x20) = (u8)((*(lcdmem + 0x20) & ~bitmask) |
                                (*lcdmem & bitmask));
    }
}

// *************************************************************************************************
// @fn          set_value
// @brief       Generic value setting routine
// @param       s32 * value						Pointer to value to set
//				u8digits						Number of digits
//				u8 blanks						Number of whitespaces before first valid digit
//				s32 limitLow					Lower limit of value
//				s32 limitHigh					Upper limit of value
//				u16 mode		
//				u8 segments					Segments where value should be drawn
//				fptr_setValue_display_function1		Value-specific display routine
// @return      none
// *************************************************************************************************
void set_value(s32 * value, u8 digits, u8 blanks, s32 limitLow, s32 limitHigh, u16 mode, u8 segments, void (*fptr_setValue_display_function1)(u8 segments, u32 value, u8 digits, u8 blanks))
{
	u8 update;
	s16 stepValue = 1;
	u8 doRound = 0;
	u8 stopwatch_state;
	u32 val;

    /* fixed71: set menus use one and only one input source: direct polling.
       This avoids simultaneous PORT2 and Timer0_A3 repeat events. */
    const u8 edit_mask = BUTTON_STAR_PIN | BUTTON_NUM_PIN | BUTTON_UP_PIN | BUTTON_DOWN_PIN;
    const u16 debounce_ticks = CONV_MS_TO_TICKS(25);
    const u16 repeat_delay_ticks = CONV_MS_TO_TICKS(800);
    const u16 repeat_period_ticks = CONV_MS_TO_TICKS(200);
    u8 saved_button_ie;
    u8 raw_edit, last_raw_edit, stable_edit;
    u16 raw_changed_at;
    u16 hold_started_at = 0;
    u16 repeat_at = 0;
    u8 held_dir = 0;
    u8 repeat_started = 0;
	
	// Clear button flags
	button.all_flags = 0;
	
	// Clear blink memory
	clear_blink_mem();
	
	// For safety only - buzzer on/off and button_repeat share same IRQ
	stop_buzzer();
	
	// Disable stopwatch display update while function is active
	stopwatch_state = sStopwatch.state;
	sStopwatch.state = STOPWATCH_HIDE;
	
	// Init step size and repeat counter
	sButton.repeats = 0;

    /* Disable menu-key PORT2 IRQs while this blocking editor is active.
       Sensor IRQ bits on PORT2 are left untouched. */
    saved_button_ie = BUTTONS_IE & edit_mask;
    BUTTONS_IE &= (u8)~edit_mask;
    BUTTONS_IFG &= (u8)~edit_mask;
    BUTTONS_IES &= (u8)~edit_mask;

    raw_edit = BUTTONS_IN & edit_mask;
    last_raw_edit = raw_edit;
    stable_edit = raw_edit;
    raw_changed_at = TA0R;

    /* The long-press key used to enter the editor may still be held.
       Do not treat it as the first edit action. */
    while (BUTTONS_IN & edit_mask)
    {
#ifdef USE_WATCHDOG
        WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#endif
    }
    raw_edit = last_raw_edit = stable_edit = 0;
    raw_changed_at = TA0R;

	// Initial display update
	update = 1;
	
	// fixed37: slow, clearly visible ~1 Hz blink (ACLK / 32768).
	set_blink_rate(BIT4 + BIT3);
	start_blink();
	
	// Value set loop
	while(1) 
	{
        /* fixed71 direct debounce and repeat.
           Press = one event after 25 ms stable.
           Hold UP/DOWN for 800 ms, then repeat every 200 ms. */
        raw_edit = BUTTONS_IN & edit_mask;
        if (raw_edit != last_raw_edit)
        {
            last_raw_edit = raw_edit;
            raw_changed_at = TA0R;
        }
        else if ((raw_edit != stable_edit) &&
                 ((u16)(TA0R - raw_changed_at) >= debounce_ticks))
        {
            u8 old_stable = stable_edit;
            u8 newly_pressed;
            stable_edit = raw_edit;
            newly_pressed = stable_edit & (u8)~old_stable;

            if (newly_pressed & BUTTON_STAR_PIN)
                button.flag.star = 1;
            if (newly_pressed & BUTTON_NUM_PIN)
                button.flag.num = 1;

            if (newly_pressed & BUTTON_UP_PIN)
            {
                button.flag.up = 1;
                held_dir = BUTTON_UP_PIN;
                hold_started_at = TA0R;
                repeat_at = TA0R;
                repeat_started = 0;
                sButton.repeats = 0;
            }
            else if (newly_pressed & BUTTON_DOWN_PIN)
            {
                button.flag.down = 1;
                held_dir = BUTTON_DOWN_PIN;
                hold_started_at = TA0R;
                repeat_at = TA0R;
                repeat_started = 0;
                sButton.repeats = 0;
            }

            if (held_dir && !(stable_edit & held_dir))
            {
                held_dir = 0;
                repeat_started = 0;
                sButton.repeats = 0;
                start_blink();
            }
        }

        if (held_dir && (stable_edit & held_dir))
        {
            if (!repeat_started)
            {
                if ((u16)(TA0R - hold_started_at) >= repeat_delay_ticks)
                {
                    repeat_started = 1;
                    repeat_at = TA0R;
                    if (held_dir == BUTTON_UP_PIN) button.flag.up = 1;
                    else button.flag.down = 1;
                    sButton.repeats++;
                    stop_blink();
                }
            }
            else if ((u16)(TA0R - repeat_at) >= repeat_period_ticks)
            {
                repeat_at = TA0R;
                if (held_dir == BUTTON_UP_PIN) button.flag.up = 1;
                else button.flag.down = 1;
                sButton.repeats++;
                stop_blink();
            }
        }

		// Idle timeout: exit function
		if (sys.flag.idle_timeout) break;

		// Button STAR (short) button: exit function
		if (button.flag.star) break;

		// NUM button: exit function and goto to next value (if available)
		if (button.flag.num)
		{
			if ((mode & SETVALUE_NEXT_VALUE) == SETVALUE_NEXT_VALUE) break;
		}

		// UP button: increase value
		if(button.flag.up)
		{
			// Increase value
			* value = * value + stepValue;
			
			// Check value limits
			if (* value > limitHigh) 
			{
				// Check if value can roll over, else stick to limit
				if ((mode & SETVALUE_ROLLOVER_VALUE) == SETVALUE_ROLLOVER_VALUE) 	* value = limitLow;
				else 																* value = limitHigh;				
					
				// Reset step size to default
				stepValue = 1;	
			}

			// Trigger display update
			update = 1;
			
			// Clear button flag
			button.flag.up = 0;
		}
		
		// DOWN button: decrease value
		if(button.flag.down)
		{
			// Decrease value
			* value = * value - stepValue;
			
			// Check value limits
			if (* value < limitLow) 
			{
				// Check if value can roll over, else stick to limit
				if ((mode & SETVALUE_ROLLOVER_VALUE) == SETVALUE_ROLLOVER_VALUE)	* value = limitHigh;
				else																* value = limitLow;
					
				// Reset step size to default
				stepValue = 1;	
			}

			// Trigger display update
			update = 1;

			// Clear button flag	
			button.flag.down = 0;
		}

		
		// When fast mode is enabled, increase step size if Sx button is continuously
		if ((mode & SETVALUE_FAST_MODE) == SETVALUE_FAST_MODE)
		{
			switch (sButton.repeats)
			{
				case 0:			stepValue = 1;		doRound = 0; 	break;
				case 10:		
				case -10:		stepValue = 10;		doRound = 1; 	break;
				case 20:			
				case -20:		stepValue = 100;	doRound = 1; 	break;
				case 30:			
				case -30:		stepValue = 1000;	doRound = 1; 	break;
			}
			
			// Round value to avoid odd numbers on display
			if (stepValue != 1 && doRound == 1)	
			{
				* value -= * value % stepValue;
				doRound = 0;
			}
		}

		// Update display when there is new data
		if (update)
		{
			/* fixed35: keep the complete screen visible while editing.
			   Only the selected field is placed into LCD blink memory below. */
			// Display up or down arrow according to sign of value
			if ((mode & SETVALUE_DISPLAY_ARROWS) == SETVALUE_DISPLAY_ARROWS)
			{
				if (* value >= 0)
				{
					display_symbol(LCD_SYMB_ARROW_UP, SEG_ON);
					display_symbol(LCD_SYMB_ARROW_DOWN, SEG_OFF);
					val = *value;
				}
				else 
				{
					display_symbol(LCD_SYMB_ARROW_UP, SEG_OFF);
					display_symbol(LCD_SYMB_ARROW_DOWN, SEG_ON);
					val = *value * (-1);
				}
			}
			else
			{
				val = *value;
			}

			// Display function can either display value directly, modify value before displaying 
			// or display a string referenced by the value
			fptr_setValue_display_function1(segments, val, digits, blanks);

			/* Make exactly the edited field blink. The normal display memory
			   remains untouched, so all other values stay continuously visible. */
			set_edit_field_blink(segments);

			// Clear update flag
			update = 0;
		}
		
		// Call idle loop to serve background tasks
		idle_loop();
		
	}
	
	// Clear up and down arrows
	display_symbol(LCD_SYMB_ARROW_UP, SEG_OFF);
	display_symbol(LCD_SYMB_ARROW_DOWN, SEG_OFF);
	
	// Stop edit-field blinking; keep the global blink rate at ~1 Hz.
	stop_blink();
	set_blink_rate(BIT4 + BIT3);
	clear_blink_mem();
	
    /* Restore menu-key wake interrupts.  Normal UI once again owns them. */
    BUTTONS_IFG &= (u8)~edit_mask;
    BUTTONS_IES &= (u8)~edit_mask;
    BUTTONS_IE = (BUTTONS_IE & (u8)~edit_mask) | saved_button_ie;

	// Enable stopwatch display updates again
	sStopwatch.state = stopwatch_state;
}
 
