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
// Time functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"

// driver
#include "ports.h"
#include "display.h"
#include "timer.h"

// logic
#include "menu.h"
#include "clock.h"
#include "alarm.h"
#include "user.h"
#include "date.h"
#include "random.h"
#include "agility.h"
#include "display2.h"
#include "display2.h"
#include "timezone.h"

// *************************************************************************************************
// Prototypes section
void reset_clock(void);
void clock_tick(void);
void mx_time(u8 line);
void sx_time(u8 line);

void calc_24H_to_12H(u8 * hours, u8 * timeAM);
void conv_24H_to_12H(u8 * hours24, u8 * hours12, u8 * timeAMorPM);

extern void init_ucs(void);


// *************************************************************************************************
// Defines section
#define DAILY_CORRECTION_SEC_DEFAULT     (0)  // 0..59 seconds
#define WEEKLY_CORRECTION_SEC_DEFAULT    (0)  // sum of daily+weekly must be in range of 0..59 seconds!
#define DST_NO      0
#define DST_AUTO    1

// *************************************************************************************************
// Global Variable section
__attribute__((noinit)) struct time sTime;

/* fixed57: optional double-beep at every full hour. Default OFF after reset. */
u8 HourlyBeepFlag = 0;
u8 TimeCorrectionFlag;
u8 DST_CorrectionFlag;
u8 DST_AutoFlag;
s8 DailyCorr;
s8 WeeklyCorr;
u8 TimeAdjustmentFlag;

// For Backlight: BL remains on for about 4sec also after button pressed
extern u8 BlOnFlag;
u8 BlTimeoutCounter = 0;

// Display values for time format selection
const u8 selection_Timeformat[][4] =
{
	"24H", "12H"
};


// *************************************************************************************************
// Extern section
extern void (*fptr_lcd_function_line1)(u8 line, u8 update);
extern void (*fptr_lcd_function_line2)(u8 line, u8 update);

extern void cdtimer_tick(void);


// *************************************************************************************************
// @fn          check_ucs
// @brief       Check the Unified Clock System.
// @param       none
// @return      none
// *************************************************************************************************
void check_ucs(void)
{
    /* GCC no-radio port: UCS recovery is intentionally disabled.
     * The runtime uses the stable clock configuration established at boot.
     * Re-entering init_ucs() from the minute tick caused stalls/resets in the
     * original IAR-specific recovery path.
     */
}



// *************************************************************************************************
// @fn          reset_clock
// @brief       Resets clock time to 00:00:00, 24H time format.
// @param       none
// @return      none
// *************************************************************************************************
void reset_clock(void) 
{
	// Set global system time to 0
	sTime.system_time = 0;
	
	// Set main 24H time to start value only if the time is invalid
  if((sTime.hour > 23) || (sTime.minute > 59) || (sTime.second > 59))
  {
  	sTime.hour   = 12; 
  	sTime.minute = 0; 
  	sTime.second = 0; 
  }
	// Display style of both lines is default (HH:MM)
	sTime.line1ViewStyle = DISPLAY_DEFAULT_VIEW;
	
	// Reset timeout detection
	sTime.last_activity 		  = 0;

	// Reset Time correction & DST flags
	TimeCorrectionFlag = 0;
	DST_CorrectionFlag = 0;
    TimeAdjustmentFlag = 0;
    
    DST_AutoFlag = DST_NO;
    DailyCorr  = (s8)DAILY_CORRECTION_SEC_DEFAULT;
    WeeklyCorr = (s8)WEEKLY_CORRECTION_SEC_DEFAULT;
}


// *************************************************************************************************
// @fn          clock_tick
// @brief       Add 1 second to system time and to display time
// @param       none
// @return      none
// *************************************************************************************************
void clock_tick(void) 
{
    // Use sTime.drawFlag to minimize display updates
	// sTime.drawFlag = 1: second
	// sTime.drawFlag = 2: minute, second
	// sTime.drawFlag = 3: hour, minute
	sTime.drawFlag = 1;

	// Increase global system time
	sTime.system_time++;

	// Add 1 second
	sTime.second++;
	
	// Add 1 minute
	if (sTime.second == 60)
	{
		// Check Unified Clock System
		check_ucs();
		
		sTime.second = 0;
		sTime.minute++;
		sTime.drawFlag++;

		// Add 1 hour	
		if (sTime.minute == 60)
		{
			sTime.minute = 0;
			sTime.hour++;
			sTime.drawFlag++;

			// Add 1 day
			if (sTime.hour == 24)
			{
				sTime.hour = 0;
				add_day();
				// Check for start of weekly correction midnight between Sunday and Monday
				if(sDate.DayOfWeek == 1) { TimeCorrectionFlag = 2; }  
				// Check for start of daily correction every midnight
				else { TimeCorrectionFlag = 1; }
			    
			    // Check for start of DST (Daylight saving time) correction
			    if(DST_AutoFlag == DST_AUTO)
			    {
			        if (sys.flag.use_metric_units)      // Europe
			        {
			            // Last Sunday in March
			            if((sDate.month == 3) && (sDate.day >= 25) && (sDate.DayOfWeek == 0))
			            {
			                DST_CorrectionFlag = 1;     // Switch to DST during summer
			            }
			            // Last Sunday in October
			            if((sDate.month == 10) && (sDate.day >= 25) && (sDate.DayOfWeek == 0))
			            {
			                DST_CorrectionFlag = 2;     // Switch to normal time during winter
			            }
			        }
			        else    // USA
			        {
			            // Second Sunday in March
			            if((sDate.month == 03) && (sDate.day >= 8) && (sDate.day <= 14) && (sDate.DayOfWeek == 0))
			            {
			                DST_CorrectionFlag = 3;     // Switch to DST during summer
			            }
			            // First Sunday in November
			            if((sDate.month == 11) && (sDate.day <= 7) && (sDate.DayOfWeek == 0))
			            {
			                DST_CorrectionFlag = 4;     // Switch to normal time during winter
			            }
			        }
			    }
			}
		}
	}
    
    // Time correction
    if(TimeCorrectionFlag != 0)
    {
        // Daily correction
        if(TimeCorrectionFlag == 1)
        {
            if((DailyCorr < 0) && (sTime.second == (u8)(DailyCorr * (-1))))
            {
                sTime.second = 0;
                TimeCorrectionFlag = 0;
            }
            if(DailyCorr > 0)
            {
                sTime.second = DailyCorr;
                TimeCorrectionFlag = 0;
            }
            if(DailyCorr == 0){ TimeCorrectionFlag = 0; }
        }
        // Weekly correction
        if(TimeCorrectionFlag == 2)
        {
            s8 WeeklyCorrTmp;
            
            WeeklyCorrTmp = WeeklyCorr + DailyCorr;
            
            if((WeeklyCorrTmp < 0) && (sTime.second == (u8)(WeeklyCorrTmp * (-1))))
            {
                sTime.second = 0;
                TimeCorrectionFlag = 0;
            }
            if(WeeklyCorr > 0)
            {
                sTime.second = (u8)WeeklyCorrTmp;
                TimeCorrectionFlag = 0;
            }
            if(WeeklyCorr == 0) { TimeCorrectionFlag = 0; }
        }
    }

    // DST (Daylight saving time) correction
    if(DST_CorrectionFlag != 0)
    {
        // Europe
        if((DST_CorrectionFlag == 1) && (sTime.hour == 2)) 
        { 
            DST_CorrectionFlag = 0; 
			sTime.hour++;
			sTime.drawFlag = 3;
        }
        if((DST_CorrectionFlag == 2) && (sTime.hour == 3)) 
        { 
            DST_CorrectionFlag = 0; 
			sTime.hour--;
			sTime.drawFlag = 3;
        }
        // USA
        if((DST_CorrectionFlag == 3) && (sTime.hour == 2)) 
        { 
            DST_CorrectionFlag = 0; 
			sTime.hour++;
			sTime.drawFlag = 3;
        }
        if((DST_CorrectionFlag == 4) && (sTime.hour == 2)) 
        { 
            DST_CorrectionFlag = 0; 
			sTime.hour--;
			sTime.drawFlag = 3;
        }
    }

	// Changes the backlight behavior: 
	// A short push of the button activates the backlight for about 4 seconds.
	if(BlOnFlag == 1)
        {
        BlTimeoutCounter++;
	    if(BlTimeoutCounter > 3)
            {
            BlOnFlag = 0;
            BlTimeoutCounter = 0;
            // display_symbol(LCD_ICON_HEART, SEG_OFF);
            BUTTONS_DIR &= ~BUTTON_BACKLIGHT_PIN;
            BUTTONS_OUT &= ~BUTTON_BACKLIGHT_PIN;
            BUTTONS_DS &= ~BUTTON_BACKLIGHT_PIN;
            }
        }
 
 // show second on bottom display line but only in DATE menu
 if((ptrMenu_L2 == &menu_L2_Date) && (sDate.display == DISPLAY_ALTERNATIVE_VIEW_4) && (TimeAdjustmentFlag == 0))
 {
    // Display second
    display_chars(LCD_SEG_L2_1_0, chronos_itoa(sTime.second, 2, 0), SEG_ON);
 }
 /* fixed47: application services are run only from main foreground. */
}


// *************************************************************************************************
// @fn          convert_hour_to_12H_format
// @brief       Convert internal 24H time to 12H time.
// @param       u8 hour		Hour in 24H format
// @return      u8				Hour in 12H format
// *************************************************************************************************
u8 convert_hour_to_12H_format(u8 hour)
{
	// 00:00 .. 11:59 --> AM 12:00 .. 11:59
	if (hour == 0)			return (hour + 12);
	else if (hour <= 12)	return (hour);	
	// 13:00 .. 23:59 --> PM 01:00 .. 11:59
	else  					return (hour - 12);
}


// *************************************************************************************************
// @fn          is_hour_am
// @brief       Checks if internal 24H time is AM or PM
// @param       u8 hour		Hour in 24H format
// @return      u8				1 = AM, 0 = PM
// *************************************************************************************************
u8 is_hour_am(u8 hour)
{
	// 00:00 .. 11:59 --> AM 12:00 .. 11:59
	if (hour < 12)	return (1);
	// 12:00 .. 23:59 --> PM 12:00 .. 11:59
	else  			return (0);
}


// *************************************************************************************************
// @fn          display_selection_Timeformat
// @brief       Display time format 12H / 24H. 
// @param       u8 segments			Target segments where to display information
//				u32 index			0 or 1, index for value string
//				u8 digits			Not used
//				u8 blanks			Not used
// @return      none
// *************************************************************************************************
void display_selection_Timeformat1(u8 segments, u32 index, u8 digits, u8 blanks)
{
	if (index < 2) display_chars(segments, (u8 *)selection_Timeformat[index], SEG_ON_BLINK_OFF);
}


// *************************************************************************************************
// @fn          mx_time
// @brief       Clock set routine.
// @param       u8 line		LINE1, LINE2
// @return      none
// *************************************************************************************************
void mx_time(u8 line)
{
	u8 select;
	s32 timeformat;
	s16 timeformat1;
	s32 hours;
	s32 minutes;
	s32 seconds;
	s32 DailyCorrTmp;
    s32 WeeklyCorrTmp;
    s32 DST_AutoFlagTmp;
    s32 HourlyBeepFlagTmp;
    s32 UTCOffsetHalfHoursTmp;
    
	u8 * str;
	
	// Clear display
    TimeAdjustmentFlag = 1;	
	clear_display_all();
	
    // Convert global time to local variables
	// Global time keeps on ticking in background until it is overwritten
	if (sys.flag.use_metric_units)
	{
		timeformat 	= TIMEFORMAT_24H;
	}
	else
	{
		timeformat 	= TIMEFORMAT_12H;
	}
	timeformat1	= timeformat;
	hours 		= sTime.hour; 
	minutes 	= sTime.minute;
	seconds 	= sTime.second;
	DailyCorrTmp  = DailyCorr;
	WeeklyCorrTmp = WeeklyCorr;
	DST_AutoFlagTmp = DST_AutoFlag;
	HourlyBeepFlagTmp = HourlyBeepFlag;
	UTCOffsetHalfHoursTmp = PrimaryUtcOffsetHalfHours;
	
	// Init value index
	select = 0;	
		
	// Loop values until all are set or user breaks	set
	while(1) 
	{
    	// Idle timeout: exit without saving 
		if (sys.flag.idle_timeout)
		{
			// Roll back time format
			if (timeformat1 == TIMEFORMAT_24H) 	sys.flag.use_metric_units = 1;
			else								sys.flag.use_metric_units = 0;
			display_symbol(LCD_SYMB_AM, SEG_OFF);
            TimeAdjustmentFlag = 0;
			break;
		}
		
		// Button STAR (short): save, then exit 
		if (button.flag.star) 
		{
			// Stop clock timer
			Timer0_Stop();

			// Store local variables in global clock time
			sTime.hour 	 = hours;
			sTime.minute = minutes;
			sTime.second = seconds;
	        DailyCorr    = DailyCorrTmp;
	        WeeklyCorr   = WeeklyCorrTmp;
	        DST_AutoFlag = DST_AutoFlagTmp;
	        timezone_set_dst_enabled((u8)DST_AutoFlagTmp);
	        HourlyBeepFlag = (u8)HourlyBeepFlagTmp;
	        PrimaryUtcOffsetHalfHours = (s8)UTCOffsetHalfHoursTmp;

			// Start clock timer
			Timer0_Start();
			
			// Full display update is done when returning from function
			display_symbol(LCD_SYMB_AM, SEG_OFF);
            TimeAdjustmentFlag = 0;
			break;
		}

		switch (select)
		{
			case 0:		// Clear LINE1 and LINE2 and AM icon - required when coming back from set_value(seconds)
						clear_display();
						display_symbol(LCD_SYMB_AM, SEG_OFF);
						
						// Set 24H / 12H time format
						set_value(&timeformat, 1, 0, 0, 1, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_SELECTION + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_1, display_selection_Timeformat1);
						
						// Modify global time format variable immediately to update AM/PM icon correctly
						if (timeformat == TIMEFORMAT_24H) 	sys.flag.use_metric_units = 1;
						else								sys.flag.use_metric_units = 0;
						select = 1;
						break;
			
			case 1:		// Display HH:MM (LINE1) and .SS (LINE2)
						str = chronos_itoa(hours, 2, 0);
						display_chars(LCD_SEG_L1_3_2, str, SEG_ON);
						display_symbol(LCD_SEG_L1_COL, SEG_ON);
						
						str = chronos_itoa(minutes, 2, 0);
						display_chars(LCD_SEG_L1_1_0, str, SEG_ON);

						str = chronos_itoa(seconds, 2, 0);
						display_chars(LCD_SEG_L2_1_0, str, SEG_ON);
						display_symbol(LCD_SEG_L2_DP, SEG_ON);
									
						// Set hours
						set_value(&hours, 2, 0, 0, 23, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_2, display_hours1);
						select = 2;
						break;
			
			case 2:		// Set minutes
						set_value(&minutes, 2, 0, 0, 59, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L1_1_0, display_value1);
						select = 3;
						break;
			
			case 3:		// Set seconds
						set_value(&seconds, 2, 0, 0, 59, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_1_0, display_value1);
						select = 4;
						break;

			case 4:		// Set daily adjusting value
						clear_display();
						display_chars(LCD_SEG_L2_4_0, (u8 *)"ADJ-1", SEG_ON);
					    set_value(&DailyCorrTmp, 2, 0, -10, 10, SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE + SETVALUE_DISPLAY_ARROWS, LCD_SEG_L1_3_2, display_value1);
						select = 5;
						break;
			
			case 5:		// Set weekly adjusting value
						display_chars(LCD_SEG_L2_4_0, (u8 *)"ADJ-7", SEG_ON);
					    set_value(&WeeklyCorrTmp, 2, 0, -30, 30, SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE + SETVALUE_DISPLAY_ARROWS, LCD_SEG_L1_3_2, display_value1);
						select = 6;
						break;
			
			case 6:		// Set DST (Daylight Saving Time) automatic
						clear_display();
						display_chars(LCD_SEG_L2_4_0, (u8 *)" DST ", SEG_ON);
					    set_value(&DST_AutoFlagTmp, 0, 0, 0, 1, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_0, display_OFF_ON);
						select = 7;
						break;

			case 7:		// Hourly full-hour double beep ON/OFF
						clear_display();
						display_chars(LCD_SEG_L2_4_0, (u8 *)"HOUR ", SEG_ON);
					    set_value(&HourlyBeepFlagTmp, 0, 0, 0, 1, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_0, display_OFF_ON);
						select = 8;
						break;

			case 8:		// Standard UTC offset in 0.5h steps
						clear_display();
						display_chars(LCD_SEG_L2_4_2, (u8 *)"UTC", SEG_ON);
					    set_value(&UTCOffsetHalfHoursTmp, 3, 0, -24, 28, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_DISPLAY_ARROWS + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_0, display_timezone_offset);
						select = 0;
						break;
		}
	}
	
	// Clear button flags
	button.all_flags = 0;
}


// *************************************************************************************************
// @fn          sx_time
// @brief       Time user routine. Toggles view style between HH:MM and SS.
// @param       line		LINE1
// @return      none
// *************************************************************************************************
void sx_time(u8 line)
{
	// Toggle display view style
	if (sTime.line1ViewStyle == DISPLAY_DEFAULT_VIEW) 	sTime.line1ViewStyle = DISPLAY_ALTERNATIVE_VIEW;
	else 									 			sTime.line1ViewStyle = DISPLAY_DEFAULT_VIEW;
}


// *************************************************************************************************
// @fn          display_time
// @brief       Clock display routine. Supports 24H and 12H time format. 
// @param       u8 line			LINE1
//				u8 update		DISPLAY_LINE_UPDATE_FULL, DISPLAY_LINE_UPDATE_PARTIAL
// @return      none
// *************************************************************************************************
void display_time(u8 line, u8 update)
{
	u8 hour12;
	
	// Partial update
 	if (update == DISPLAY_LINE_UPDATE_PARTIAL) 
	{
		if(sTime.drawFlag != 0) 
		{
			if (sTime.line1ViewStyle == DISPLAY_DEFAULT_VIEW)
			{
				switch(sTime.drawFlag) 
				{
					case 3: if (sys.flag.use_metric_units)
							{
								// Display 24H time "HH" 
								display_chars(switch_seg(line, LCD_SEG_L1_3_2, LCD_SEG_L2_3_2), chronos_itoa(sTime.hour, 2, 0), SEG_ON);
							}
							else
							{
								// Display 12H time "HH" + AM/PM
								hour12 = convert_hour_to_12H_format(sTime.hour);
								display_chars(switch_seg(line, LCD_SEG_L1_3_2, LCD_SEG_L2_3_2), chronos_itoa(hour12, 2, 0), SEG_ON); 
								display_am_pm_symbol(sTime.hour);
							}
							
					case 2: display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), chronos_itoa(sTime.minute, 2, 0), SEG_ON);

		// fixed49: show alarm icon on the normal clock display only when enabled.
		if (line == LINE1)
		{
			if (sAlarm.state == ALARM_ENABLED || sAlarm.state == ALARM_ON)
				display_symbol(LCD_ICON_ALARM, SEG_ON_BLINK_OFF);
			else
				display_symbol(LCD_ICON_ALARM, SEG_OFF_BLINK_OFF);
		}
 
				}
			}
			else
			{
				// Seconds are always updated
				display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), chronos_itoa(sTime.second, 2, 0), SEG_ON);
			}
		}	
	}
	else if (update == DISPLAY_LINE_UPDATE_FULL)			
	{
		// Full update
		if (sTime.line1ViewStyle == DISPLAY_DEFAULT_VIEW)
		{	
			// Display 24H/12H time
			if (sys.flag.use_metric_units)
			{
				// Display 24H time "HH" 
				display_chars(switch_seg(line, LCD_SEG_L1_3_2, LCD_SEG_L2_3_2), chronos_itoa(sTime.hour, 2, 0), SEG_ON);
			}
			else
			{
				// Display 12H time "HH" + AM/PM information
				hour12 = convert_hour_to_12H_format(sTime.hour);
				display_chars(switch_seg(line, LCD_SEG_L1_3_2, LCD_SEG_L2_3_2), chronos_itoa(hour12, 2, 0), SEG_ON); 
				// Display AM/PM information
				if (line == LINE1)
				{
					display_am_pm_symbol(sTime.hour);
				}
			}
							
			// Display minute
			display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), chronos_itoa(sTime.minute, 2, 0), SEG_ON); 
			display_symbol(switch_seg(line, LCD_SEG_L1_COL, LCD_SEG_L2_COL0), SEG_ON_BLINK_OFF);
		}
		else
		{
			// Display seconds
			display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), chronos_itoa(sTime.second, 2, 0), SEG_ON);
			display_symbol(switch_seg(line, LCD_SEG_L1_DP1, LCD_SEG_L2_DP), SEG_ON);
		}

		/* fixed50: persistent alarm icon on every full Time redraw. */
		if (line == LINE1)
		{
			if (sAlarm.state == ALARM_ENABLED || sAlarm.state == ALARM_ON)
				display_symbol(LCD_ICON_ALARM, SEG_ON_BLINK_OFF);
			else
				display_symbol(LCD_ICON_ALARM, SEG_OFF_BLINK_OFF);
		}
	}
	else if (update == DISPLAY_LINE_CLEAR)
	{
		display_symbol(switch_seg(line, LCD_SEG_L1_COL, LCD_SEG_L2_COL0), SEG_OFF_BLINK_OFF);
		// Change display style to default (HH:MM)
		sTime.line1ViewStyle = DISPLAY_DEFAULT_VIEW;
		// Clean up AM/PM icon
		display_symbol(LCD_SYMB_AM, SEG_OFF);
	}
}
