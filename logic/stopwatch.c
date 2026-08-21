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
// Stopwatch functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"
#include <string.h>

// driver
#include "stopwatch.h"
#include "ports.h"
#include "display.h"
#include "timer.h"
#include "user.h"

// logic
#include "menu.h"


// *************************************************************************************************
// Prototypes section
void start_stopwatch(void);
void stop_stopwatch(void);
void reset_stopwatch(void);
void stopwatch_tick(void);
void update_stopwatch_timer(void);
void mx_stopwatch(u8 line);
void sx_stopwatch(u8 line);
void display_stopwatch(u8 line, u8 update);
void set_stopwatchmode(void);


// *************************************************************************************************
// Defines section
#define MODE_STOPTIMER   0
#define MODE_LAPTIMER    1
#define LAP_RUNNING      0
#define LAP_CATCHED      1

// *************************************************************************************************
// Global Variable section
struct stopwatch sStopwatch;
u8     StopwatchMode;
u8	   LAP_Time[8];
u8	   LAP_TimeFlag;



// *************************************************************************************************
// Extern section


// *************************************************************************************************
// @fn          init_stopwatch
// @brief       Clears stopwatch counter and sets stopwatch state to off.
// @param       none
// @return      none
// *************************************************************************************************
void init_stopwatch(void)
{
	// Clear counter
	memcpy(sStopwatch.time, "00000000", sizeof(sStopwatch.time));
    memcpy(LAP_Time, sStopwatch.time, sizeof(LAP_Time));
    
	// Clear trigger
	sStopwatch.swtIs10Hz 	= 0;		// 1/10Hz trigger
	sStopwatch.swtIs1Hz  	= 0;		// 1Hz trigger
	
	// Init stopwatch state 'Off'
	sStopwatch.state 	  	= STOPWATCH_STOP;		
	
	// Default display style is MM:SS:HH
	sStopwatch.viewStyle 	= DISPLAY_DEFAULT_VIEW;
}


// *************************************************************************************************
// @fn          reset_stopwatch
// @brief       Init stopwatch and sets mode & LAP flag.
// @param       none
// @return      none
// *************************************************************************************************
void reset_stopwatch(void)
{
	init_stopwatch();
	StopwatchMode = MODE_STOPTIMER; 
	LAP_TimeFlag  = LAP_CATCHED;
}


// *************************************************************************************************
// @fn          update_stopwatch_timer
// @brief       Set new compare time for next 1/1Hz or 1/100Hz interrupt. Takes care for exact 1 second timing.
// @param       ticks (1 tick = 1/32768 sec)
// @return      none
// *************************************************************************************************
void update_stopwatch_timer(void)
{
  //Make sure Stopwatch is running before we do anything else (this might slightly mess up my timing, but oh well)
  if(sStopwatch.state == STOPWATCH_RUN){
	u16 value;

	// Load CCR register with next capture time
	if (sStopwatch.viewStyle == DISPLAY_DEFAULT_VIEW) 
	{
		// Timer interrupts occur every 32768/100 = 328 ACLK
		// --> stopwatch runs too slow (1 sec nominal != 100 interupts * 328 ACLK = 32800 ACLK = 1.00098 sec)
		// --> ideally correct timer value every 10 ticks by (32768 - 32800)/10 = 3.2
		// --> correct timer value every 10Hz by 3, 
		// --> correct timer value every 1Hz correct by 5
		value = TA0CCR2 + STOPWATCH_100HZ_TICK;

		if (sStopwatch.swtIs1Hz) 
		{
			value -= 5;
			sStopwatch.swtIs1Hz = 0;	
			sStopwatch.swtIs10Hz = 0;	
		}
		else if (sStopwatch.swtIs10Hz) 
		{
			value -= 3;
			sStopwatch.swtIs10Hz = 0;	
		}
	}
	else // Alternative view
	{
		// Timer interrupts occur every 32768/1 = 32768 ACLK
		value = TA0CCR2 + STOPWATCH_1HZ_TICK;
	}
	
	// Update CCR
	TA0CCR2 = value;  
  } 
}


// *************************************************************************************************
// @fn          stopwatch_tick
// @brief       Called by 1/100Hz interrupt handler. 
//				Increases stopwatch counter and triggers display update.
// @param       none
// @return      none
// *************************************************************************************************
void stopwatch_tick(void)
{
  //Make sure Stopwatch is running before we do anything else (this might slightly mess up my timing, but oh well)
  if(sStopwatch.state == STOPWATCH_RUN){
	static u8 delay = 0;
	
	// Default view (< 20 minutes): display and count MM:SS:hh
	if (sStopwatch.viewStyle == DISPLAY_DEFAULT_VIEW)
	{
		// Add 1/100 sec 
		sStopwatch.time[7]++;
				
		// Draw flag minimizes display update activity
		//
		// swt.drawFlag = 1: second L
		// swt.drawFlag = 2: second H/L
		// swt.drawFlag = 3: minutes L, second H/L
		// swt.drawFlag = 4: minutes H/L, second H/L
		// swt.drawFlag = 5: hours L, minutes H/L, second H/L
		// swt.drawFlag = 6: hours H/L, minutes H/L, second H/L
		// swt.drawFlag = 7: 1/10 sec, 1/100 sec
		// swt.drawFlag = 8: 1/100 sec (every 17/100 sec to reduce display draw activity)
		if (delay++ > 17) 
		{
			sStopwatch.drawFlag = 8;
			delay = 0;
		}
	
		// Add 1/10 sec 
		if (sStopwatch.time[7] == 0x3A)
		{
			sStopwatch.time[7]='0';
			sStopwatch.time[6]++;
			
			// 1/10Hz trigger 
			sStopwatch.swtIs10Hz = 1;
			
			// Update draw flag
			sStopwatch.drawFlag = 7;
		}
	} 
	else // Alternative view (20 minutes .. 20 hours): display and count HH:MM:SS
	{
		// Just add 1 second
		sStopwatch.time[6] = 0x3A;
	}
			
	// Second overflow?
	if (sStopwatch.time[6] == 0x3A)
	{
		// Reset draw flag
		sStopwatch.drawFlag = 1;

		// 1Hz trigger 
		sStopwatch.swtIs1Hz = 1;
		
		// Add data
		sStopwatch.time[6]='0';
		sStopwatch.time[5]++;							// second  L (0 - 9)
		if (sStopwatch.time[5] == 0x3A) 
		{
			sStopwatch.drawFlag++;						// 2
			sStopwatch.time[5] = '0';
			sStopwatch.time[4]++;						// second  H (0 - 5)
			if (sStopwatch.time[4] == '6') 
			{
				sStopwatch.drawFlag ++;					// 3
				sStopwatch.time[4] = '0';
				sStopwatch.time[3]++;					// minutes L (0 - 9)
				if (sStopwatch.time[3] == 0x3A) 
				{
					sStopwatch.drawFlag++;				// 4
					sStopwatch.time[3] = '0';
					sStopwatch.time[2]++;				// minutes H (0 - 5)
					if (sStopwatch.time[2] == '2')
					{
						// SWT display changes from MM:SS:hh to HH:MM:SS when reaching 20 minutes 
						sStopwatch.viewStyle = DISPLAY_ALTERNATIVE_VIEW;
						display_stopwatch(LINE2, DISPLAY_LINE_UPDATE_FULL);
						
	   				} 
					else if (sStopwatch.time[2] == '6') 
					{
						sStopwatch.drawFlag++;				// 5
						sStopwatch.time[2] = '0';
						sStopwatch.time[1]++;				// hours L (0-9)	

						if (sStopwatch.time[1] == 0x3A) 
						{
							sStopwatch.drawFlag++;			// 6
							sStopwatch.time[1] = '0';
							sStopwatch.time[0]++;			// hours H (0-1)	

							if (sStopwatch.time[0] == '2') 
							{
								// When reaching 20 hours, start over 
								init_stopwatch();
								sStopwatch.state = STOPWATCH_RUN;
								display_stopwatch(LINE2, DISPLAY_LINE_UPDATE_FULL);
							}
		   				} 
	   				} 
				}
			}
		}
	}		
	
	// Always set display update flag
	display.flag.update_stopwatch = 1;
  }
}


// *************************************************************************************************
// @fn          is_stopwatch
// @brief       Is stopwatch operating and visible?
// @param       none
// @return      1=STOPWATCH_RUN, 0=other states
// *************************************************************************************************
u8 is_stopwatch(void)
{
	return ((sStopwatch.state == STOPWATCH_RUN) && (ptrMenu_L2 == &menu_L2_Stopwatch));
}


// *************************************************************************************************
// @fn          start_stopwatch
// @brief       Starts stopwatch timer interrupt and sets stopwatch state to on.
// @param       none
// @return      none
// *************************************************************************************************
void start_stopwatch(void)
{
	// Set stopwatch run flag
	sStopwatch.state = STOPWATCH_RUN;	

	// Init CCR register with current time
	TA0CCR2 = TA0R;
		
	// Load CCR register with next capture time
	update_stopwatch_timer();

	// Reset IRQ flag    
	TA0CCTL2 &= ~CCIFG; 
	          
	// Enable timer interrupt    
	TA0CCTL2 |= CCIE; 
	
	// Set stopwatch icon
	display_symbol(LCD_ICON_STOPWATCH, SEG_ON);
}


// *************************************************************************************************
// @fn          stop_stopwatch
// @brief       Stops stopwatch timer interrupt and sets stopwatch state to off.
//				Does not reset stopwatch count.
// @param       none
// @return      none
// *************************************************************************************************
void stop_stopwatch(void)
{
	if(StopwatchMode == MODE_STOPTIMER)
	{
	    // Clear timer interrupt enable   
	    TA0CCTL2 &= ~CCIE; 
        
	    // Clear stopwatch run flag
	    sStopwatch.state = STOPWATCH_STOP;	
	    
	    // Clear stopwatch icon
	    display_symbol(LCD_ICON_STOPWATCH, SEG_OFF);
    }
    else // MODE_LAPTIMER
    {
        if(LAP_TimeFlag == LAP_RUNNING)
		    {
		        // Save LAP time
		        memcpy(LAP_Time, sStopwatch.time, sizeof(LAP_Time));
		        LAP_TimeFlag = LAP_CATCHED;
		    }
		    else // LAP_CATCHED
		    {
		        LAP_TimeFlag = LAP_RUNNING;
		    }
    }
	// Call draw routine immediately
	display_stopwatch(LINE2, DISPLAY_LINE_UPDATE_FULL);
}


// *************************************************************************************************
// @fn          mx_stopwatch
// @brief       Stopwatch set routine. Mx stops stopwatch and resets count.
// @param       u8 line	LINE2
// @return      none
// *************************************************************************************************
void mx_stopwatch(u8 line)
{
	// Stop stopwatch
	stop_stopwatch();
			
	// Reset stopwatch count
	init_stopwatch();	
	LAP_TimeFlag = LAP_CATCHED;
	
	// Set mode
	set_stopwatchmode();
	
	// Display "00:00:00"
	display_stopwatch(line, DISPLAY_LINE_UPDATE_FULL);
}


// *************************************************************************************************
// @fn          sx_stopwatch
// @brief       Stopwatch direct function. Button DOWN starts/stops stopwatch, but does not reset count.
// @param       u8 line	LINE2
// @return      none
// *************************************************************************************************
void sx_stopwatch(u8 line)
{
	// DOWN: RUN, STOP
	if(button.flag.down)
	{
	    if (sStopwatch.state == STOPWATCH_STOP)
	    {
	    	// (Re)start stopwatch
	    	start_stopwatch();
	    	if(StopwatchMode == MODE_LAPTIMER) { LAP_TimeFlag = LAP_RUNNING; }
	    }
	    else if (sStopwatch.state == STOPWATCH_RUN)
	    {
	    	// fixed34: foreground button handling no longer uses the old
	    	// PORT2 ISR, so stop the stopwatch explicitly here.
	    	stop_stopwatch();
	    }
	}
}


// *************************************************************************************************
// @fn          display_stopwatch
// @brief       Stopwatch user routine. Sx starts/stops stopwatch, but does not reset count.
// @param       u8 line	LINE2
//				u8 update	DISPLAY_LINE_UPDATE_PARTIAL, DISPLAY_LINE_UPDATE_FULL
// @return      none
// *************************************************************************************************
void display_stopwatch(u8 line, u8 update)
{
	u8 DisplayTime[8];
	
	if((StopwatchMode == MODE_LAPTIMER) && (LAP_TimeFlag == LAP_CATCHED))
	{
	    memcpy(DisplayTime, LAP_Time, sizeof(DisplayTime));
	}
	else
	{
	    memcpy(DisplayTime, sStopwatch.time, sizeof(DisplayTime));
	}
	
	// Partial line update only
	if (update == DISPLAY_LINE_UPDATE_PARTIAL)
	{	
		if (display.flag.update_stopwatch)
		{
			if (sStopwatch.viewStyle == DISPLAY_DEFAULT_VIEW)
			{
				// Display MM:SS:hh

				// Check draw flag to minimize workload
				if(sStopwatch.drawFlag != 0) 
				{
					switch(sStopwatch.drawFlag) 
					{
						case 4: display_char(LCD_SEG_L2_5, DisplayTime[2], SEG_ON);
						case 3: display_char(LCD_SEG_L2_4, DisplayTime[3], SEG_ON);
						case 2: display_char(LCD_SEG_L2_3, DisplayTime[4], SEG_ON);
						case 1: display_char(LCD_SEG_L2_2, DisplayTime[5], SEG_ON);
						case 7: display_char(LCD_SEG_L2_1, DisplayTime[6], SEG_ON);
						case 8:	display_char(LCD_SEG_L2_0, DisplayTime[7], SEG_ON);
					}
				}
			}
			else // DISPLAY_ALTERNATIVE_VIEW
			{
				// Display HH:MM:SS
				switch(sStopwatch.drawFlag) 
				{
					case 6: display_char(LCD_SEG_L2_5, DisplayTime[0], SEG_ON);
					case 5: display_char(LCD_SEG_L2_4, DisplayTime[1], SEG_ON);
					case 4: display_char(LCD_SEG_L2_3, DisplayTime[2], SEG_ON);
					case 3: display_char(LCD_SEG_L2_2, DisplayTime[3], SEG_ON);
					case 2: display_char(LCD_SEG_L2_1, DisplayTime[4], SEG_ON);
					case 1: display_char(LCD_SEG_L2_0, DisplayTime[5], SEG_ON);
				}		
			}
		}
	}
	// Redraw whole line
	else if (update == DISPLAY_LINE_UPDATE_FULL)	
	{
		if (sStopwatch.viewStyle == DISPLAY_DEFAULT_VIEW)
		{
			// Display MM:SS:hh
			display_chars(LCD_SEG_L2_5_0, DisplayTime+2, SEG_ON);
		}
		else // DISPLAY_ALTERNATIVE_VIEW
		{
			// Display HH:MM:SS
			display_chars(LCD_SEG_L2_5_0, DisplayTime, SEG_ON);
		}
		if(StopwatchMode == MODE_LAPTIMER)
	    {
	        display_symbol(LCD_SEG_L2_COL1, SEG_ON_BLINK_OFF);
	        display_symbol(LCD_SEG_L2_COL0, SEG_ON_BLINK_OFF);
        }
		else // MODE_STOPTIMER
		{
		    display_symbol(LCD_SEG_L2_COL1, SEG_ON);
		    display_symbol(LCD_SEG_L2_COL0, SEG_ON);
	    }
		// Set stopwatch icon
	    display_symbol(LCD_ICON_STOPWATCH, SEG_ON_BLINK_OFF);
	}
	
	else if (update == DISPLAY_LINE_CLEAR)
	{
		// Clean up symbols when leaving function
		if (sStopwatch.state == STOPWATCH_STOP) 
		{
			display_symbol(LCD_ICON_STOPWATCH, SEG_OFF_BLINK_OFF);
		}
		else
		{
			display_symbol(LCD_ICON_STOPWATCH, SEG_ON_BLINK_OFF);
		}
	    display_symbol(LCD_SEG_L2_COL1, SEG_OFF_BLINK_OFF);
	    display_symbol(LCD_SEG_L2_COL0, SEG_OFF_BLINK_OFF);
	}
}

// *************************************************************************************************
// @fn          display_special3
// @brief       Display routine for special text
// @param       u8 segments		LCD segments where value is displayed - not used
//				u32 value	    see below
//				u8 digits		Number of digits to convert - not used
//				u8 blanks		Number of leadings blanks in itoa result string - not used
// @return      none
// *************************************************************************************************
void display_special3(u8 segments, u32 value, u8 digits, u8 blanks)
{
	if(value == 0) { display_chars(LCD_SEG_L2_3_0, (u8 *)"STOP", SEG_ON_BLINK_OFF); }
	if(value == 1) { display_chars(LCD_SEG_L2_3_0, (u8 *)"LAP ", SEG_ON_BLINK_OFF); }
}

// *************************************************************************************************
// @fn          set_stopwatch
// @brief       Set's stopwatch mode.
// @param       none
// @return      none
// *************************************************************************************************
void set_stopwatchmode(void)
{
    s32 StopwatchModeTmp;
         
    // Copy to temp 
    StopwatchModeTmp = StopwatchMode;
    
    // Loop values until all are set or user breaks set
    while(1) 
    {
        // Idle timeout: exit without saving 
        if (sys.flag.idle_timeout) break;
        
        // NUM Button (short): save, then exit 
        if (button.flag.num) 
        {
            // Store local variables in global 
            StopwatchMode = StopwatchModeTmp;
            //Set display update flag
            display.flag.full_update = 1;
            break;
        }

        // Set stopwatch mode
        clear_display_all();
        display_chars(LCD_SEG_L1_3_0, (u8 *)"MODE", SEG_ON);
        set_value(&StopwatchModeTmp, 0, 0, 0, 1, SETVALUE_DISPLAY_VALUE + SETVALUE_ROLLOVER_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_3_0, display_special3);
    }
    // Clear button flag
    button.all_flags = 0;
}


