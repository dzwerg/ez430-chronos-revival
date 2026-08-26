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
// Battery voltage measurement functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"

// driver
#include "display.h"
#include "ports.h"
#include "adc12.h"

// logic
#include "menu.h"
#include "battery.h"


// *************************************************************************************************
// Prototypes section
void reset_batt_measurement(void);
void battery_measurement(void);


// *************************************************************************************************
// Defines section


// *************************************************************************************************
// Global Variable section
struct batt sBatt;
static u8 batt_has_real_sample = 0;

static void display_battery_value_line2(void)
{
	u8 *digits = chronos_itoa(sBatt.voltage, 3, 0);

	/* L2 decimal point lies between positions 2 and 1: 327 -> 3.27.
	   The dedicated voltage symbol supplies the unit. */
	display_chars(LCD_SEG_L2_2_0, digits, SEG_ON);
	display_symbol(LCD_SEG_L2_DP, SEG_ON);
}


// *************************************************************************************************
// Extern section
extern void (*fptr_lcd_function_line2)(u8 line, u8 update);


// *************************************************************************************************
// @fn          reset_temp_measurement
// @brief       Reset temperature measurement module.
// @param       none
// @return      none
// *************************************************************************************************
void reset_batt_measurement(void)
{
	// Set flag to off
  	sBatt.state = MENU_ITEM_NOT_VISIBLE; 
	
	// Reset lobatt display counter
	sBatt.lobatt_display = BATTERY_LOW_MESSAGE_CYCLE;
	
	// Start with battery voltage of 3.00V 
	sBatt.voltage = 300;
	batt_has_real_sample = 0;
}


// *************************************************************************************************
// @fn          battery_measurement
// @brief       Init ADC12. Do single conversion of AVCC voltage. Turn off ADC12.
// @param       none
// @return      none
// *************************************************************************************************
void battery_measurement(void)
{
	u16 voltage;
	
	// Convert external battery voltage (ADC12INCH_11=AVCC-AVSS/2)
	//voltage = adc12_single_conversion(REFVSEL_2, ADC12SHT0_10, ADC12SSEL_0, ADC12SREF_1, ADC12INCH_11, ADC12_BATT_CONVERSION_TIME_USEC);
	voltage = adc12_single_conversion(REFVSEL_1, ADC12SHT0_10, ADC12INCH_11);

	// fixed78: convert ADC11 (AVCC/2) against the 2.0-V reference directly
	// to centivolts.  Use exact 32-bit arithmetic instead of the old /41
	// approximation.
	voltage = (u16)(((u32)voltage * 400UL + 2047UL) / 4095UL);

	/* IMPORTANT:
	   Do NOT add sBatt.offset here.  That value belongs to the legacy
	   TI/OpenChronos calibration path and may contain -10 (= -0.10 V) or an
	   old INFO-D calibration value.  Applying it to the new synchronous ADC
	   path caused the battery voltage to be corrected twice. */
	
	// Discard values that are clearly outside the measurement range 
	if (voltage > BATTERY_HIGH_THRESHOLD) 
	{
		voltage = sBatt.voltage;
	}
	
	// Filter battery voltage
	if (!batt_has_real_sample)
	{
		/* First real ADC sample must replace the 3.00 V boot placeholder. */
		sBatt.voltage = voltage;
		batt_has_real_sample = 1;
	}
	else
	{
		/* Light smoothing for later samples without hiding real changes. */
		sBatt.voltage = ((voltage*3) + (sBatt.voltage*7))/10;
	}

	// If battery voltage falls below low battery threshold, set system flag and modify LINE2 display function pointer
	if (sBatt.voltage < BATTERY_LOW_THRESHOLD)
	{
		sys.flag.low_battery = 1;
	}
	else
	{
		sys.flag.low_battery = 0;
	}
	// Battery is a Line1 sensor menu item.
	display.flag.line1_full_update = 1;
	
	// Indicate to display function that new value is available
	display.flag.update_battery_voltage = 1;
}




// *************************************************************************************************
// @fn          display_battery_V
// @brief       Display routine for battery voltage. 
// @param       u8 line		LINE2
//				u8 update		DISPLAY_LINE_UPDATE_FULL, DISPLAY_LINE_CLEAR
// @return      none
// *************************************************************************************************
void display_battery_V(u8 line, u8 update)
{
	// Redraw line
	if (update == DISPLAY_LINE_UPDATE_FULL)	
	{
		// Set battery and V icon
		display_symbol(LCD_SYMB_BATTERY, SEG_ON);

		// Menu item is visible
		sBatt.state = MENU_ITEM_VISIBLE; 
		
		// Fixed title on top, voltage and unit on the lower line.
		display_chars(LCD_SEG_L1_3_0, (u8 *)"BATT", SEG_ON);
		display_battery_value_line2();
	}
	else if (update == DISPLAY_LINE_UPDATE_PARTIAL)
	{
		// Refresh the measured voltage while keeping the fixed title.
		display_symbol(LCD_SYMB_BATTERY, SEG_ON);
		display_battery_value_line2();
			
		display.flag.update_battery_voltage = 0;
	}
	else if (update == DISPLAY_LINE_CLEAR)
	{
		// Menu item is not visible
		sBatt.state = MENU_ITEM_NOT_VISIBLE; 		
		
		// Clear function-specific symbols
		display_symbol(LCD_SYMB_BATTERY, SEG_OFF);
		display_symbol(LCD_SEG_L2_DP, SEG_OFF);

		/* BATT temporarily owns Line2.  Restore its selected menu item when
		   the user leaves the battery screen. */
		display.flag.line2_full_update = 1;
	}
}
