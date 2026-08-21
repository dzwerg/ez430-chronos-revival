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
// Miscellaneous functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"
#include <string.h>


// driver
#include "display.h"
#include "ports.h"
#include "vti_ps.h"


// logic
#include "miscellaneous.h"
#include "menu.h"
#include "user.h"
#include "display2.h"
#include "clock.h"
#include "date.h"
#include "alarm.h"
#include "temperature.h"
#include "altitude.h"
#include "acceleration.h"
#include "countdowntimer.h"
#include "agility.h"
#include "stopwatch.h"
#include "random.h"


// *************************************************************************************************
// Prototypes section
extern void init_ucs(void);


// *************************************************************************************************
// Defines section
#define NUMBERSTORAGE_MAX   11
#define STATE_UNLOCKED   	0
#define STATE_TEMP_UNLOCKED 1
#define STATE_LOCKED   		2

// All these defines are presets for MY watch or MY preferences
// clock.c
#define MY_DAILY_CORRECTION_SEC     (-2)  // 0..59 seconds
#define MY_WEEKLY_CORRECTION_SEC    (-3)  // sum of daily+weekly must be in range of 0..59 seconds!

// altitude.c
#define MY_AMBIENT_PRESSURE_OFFSET     -2 
#define MY_ALTITUDE                    30

// temperature.h
#define MY_TEMPERATURE_OFFSET     40      // add 04.0�C


// *************************************************************************************************
// Global Variable section
extern s8 DailyCorr;
extern s8 WeeklyCorr;
extern u8 DST_AutoFlag;
extern s16 AmbientPressureOffset;

struct UcsResetHandling sUcsResetHandling;

struct numstore
{
	u8  State;
	u16 Mem[NUMBERSTORAGE_MAX];     // Mem[0] used as key (storage locked if key != 0)
	u8  Index;
};
struct numstore sNumStore;



// *************************************************************************************************
// @fn          reset_number_storage
// @brief       Initialize number storage.
// @param       none
// @return      none
// *************************************************************************************************
void reset_number_storage(void)
{
    u8 i;
    
	sNumStore.State = STATE_UNLOCKED;
    sNumStore.Mem[0] = 0;   // key = 0 -> storage unlocked!
	sNumStore.Index = 0;
    for(i = 1; i < NUMBERSTORAGE_MAX; i++) { sNumStore.Mem[i] = i; }
}


// *************************************************************************************************
// @fn          number_storage_tick
// @brief       Check if number storage must lock.
// @param       none
// @return      none
// *************************************************************************************************
void number_storage_tick(void)
{
    // Check if number storage is displayed
    if(ptrMenu_L2 != &menu_L2_number_storage)
    {
        if(sNumStore.Mem[0] != 0) 
             { 
                 sNumStore.State = STATE_LOCKED; 
                 sNumStore.Index = 0;
             }
    }
}


// *************************************************************************************************
// @fn          sx_number_storage
// @brief       Direct function for number storage. DOWN Button ...
// @param       u8 line LINE2
// @return      none
// *************************************************************************************************
void sx_number_storage(u8 line)
{
   // DOWN Button: RUN, STOP
   if(button.flag.down)
   {
       if (sNumStore.State != STATE_LOCKED)
       {
		   if(sNumStore.Index < NUMBERSTORAGE_MAX-1) { sNumStore.Index++; }
		   else { sNumStore.Index = 0; }
	   }
   }
}


// *************************************************************************************************
// @fn          mx_number_storage
// @brief       Set number in storage.
// @param       u8 line - ignored parameter
// @return      none
// *************************************************************************************************
void mx_number_storage(u8 line)
{
    u8 * str;
    s32 NumberTmp;
    
    // Prepare display
    clear_display_all();
    // Display a "M" for "Memory"
    display_char(LCD_SEG_L1_3, 'M', SEG_ON);
    // Display storage index
    str = chronos_itoa(sNumStore.Index, 2, 1);
    display_chars(LCD_SEG_L1_1_0, str, SEG_ON);

    // Copy to temp
    if((sNumStore.Index == 0) && (sNumStore.Mem[0] != 0) && (sNumStore.State == STATE_LOCKED)) { NumberTmp = 0; }
    else { NumberTmp = sNumStore.Mem[sNumStore.Index]; }
   
    // Loop until strage is set or user breaks set
    while(1) 
    {
        // Idle timeout: exit without saving 
        if (sys.flag.idle_timeout) break;
       
        // NUM Button (short): save, then exit 
        if (button.flag.num) 
        {
            // Check key for locking mechanism
            if(sNumStore.Index == 0)
            {
                switch (sNumStore.State)
                {                
                case STATE_UNLOCKED:        if(NumberTmp != 0) 
                                            {   
                                                sNumStore.State = STATE_TEMP_UNLOCKED;
                                                sNumStore.Mem[sNumStore.Index] =  NumberTmp;
                                            }
                                            break;
                case STATE_TEMP_UNLOCKED:   if(NumberTmp != sNumStore.Mem[0]) 
                                            { 
                                                sNumStore.Mem[sNumStore.Index] =  NumberTmp;
                                            }
                                            if(NumberTmp == 0) 
                                            {   
                                                sNumStore.State = STATE_UNLOCKED;
                                                sNumStore.Mem[sNumStore.Index] =  NumberTmp;
                                            }
                                            break;
                case STATE_LOCKED:          if(NumberTmp == sNumStore.Mem[0]) 
                                            { 
                                                sNumStore.State = STATE_TEMP_UNLOCKED; 
                                            }
                                            break;
                default:                    sNumStore.State = STATE_LOCKED;
                                            sNumStore.Index = 0;
                                            break;
                }
            }
            else
            {
                // Store local variables in global 
                sNumStore.Mem[sNumStore.Index] =  NumberTmp;
            }
            
            display_symbol(LCD_SYMB_TOTAL, SEG_ON_BLINK_OFF);
            //Set display update flag
            display.flag.full_update = 1;
            break;
        }
                    
        // Display/change the storage content
        set_value(&NumberTmp, 4, 0, 0, 9999, SETVALUE_DISPLAY_VALUE + SETVALUE_FAST_MODE + SETVALUE_ROLLOVER_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_3_0, display_value1);  
    }
    // Clear button flag
    button.all_flags = 0;
}



// *************************************************************************************************
// @fn          display_number_storage
// @brief       User routine for number storage. UP/DOWN Button ...
// @param       u8 line LINE2
//              u8 update   DISPLAY_LINE_UPDATE_PARTIAL, DISPLAY_LINE_UPDATE_FULL, DISPLAY_LINE_CLEAR
// @return      none
// *************************************************************************************************
void display_number_storage(u8 line, u8 update)
{
    u8 * str;
    s32 delay;
    
    // Display storage content
    if((update == DISPLAY_LINE_UPDATE_PARTIAL) || (update == DISPLAY_LINE_UPDATE_FULL))
    {
	    // Show TOTAL icon
	    display_symbol(LCD_SYMB_TOTAL, SEG_ON);
        
        // Display a "M" for "Memory"
        display_chars(LCD_SEG_L2_3_2, "M ", SEG_ON);
        
        // Display storage index
        str = chronos_itoa(sNumStore.Index, 2, 1);
        display_chars(LCD_SEG_L2_1_0, str, SEG_ON);
        
        // Wait about 0.5sec
        delay = 800000l;
        while(delay-- != 0) { __no_operation(); }
        
        // Display now the storage content
        if(sNumStore.State == STATE_LOCKED)
        {
            display_chars(LCD_SEG_L2_3_0, (u8*)"----", SEG_ON_BLINK_ON);
        }
        else // STATE_UNLOCKED or STATE_TEMP_UNLOCKED
        {
            // Display storage content
            str = chronos_itoa(sNumStore.Mem[sNumStore.Index], 4, 0);
            display_chars(LCD_SEG_L2_3_0, str, SEG_ON);
        }
    }
    else if (update == DISPLAY_LINE_CLEAR)
    {
        // Clean up display
        display_symbol(LCD_SYMB_TOTAL, SEG_OFF);
        display_chars(LCD_SEG_L2_3_0, (u8*)"    ", SEG_OFF_BLINK_OFF);
    }

}












// *************************************************************************************************
// @fn          SetMyDefaults
// @brief       Set my default preferences
// @param       none
// @return      none
// *************************************************************************************************
void SetMyDefaults(void)
{
    DailyCorr               = MY_DAILY_CORRECTION_SEC;
    WeeklyCorr              = MY_WEEKLY_CORRECTION_SEC;
    DST_AutoFlag            = 1;    // DST_AUTO

    AmbientPressureOffset   = MY_AMBIENT_PRESSURE_OFFSET;
    sAlt.altitude           = MY_ALTITUDE;
    // Update pressure table
    update_pressure_table(sAlt.altitude, sAlt.pressure, sAlt.temperature);
    
    //Set cdtimer to a 6:30 minute default
    memcpy(scdtimer.defaultTime, "000630", sizeof(scdtimer.time));
    reset_cdtimer();
    
    // Set temperature offset
    sTemp.offset += MY_TEMPERATURE_OFFSET;
    
    // Automatic reset after Unified Clock System failed
    sUcsResetHandling.status = UCSRESET_AUTO;
}



// *************************************************************************************************
// @fn          ResetWatch
// @brief       Reset watch 
// @param       none
// @return      none
// *************************************************************************************************
void ResetWatch(void)
{
     __disable_interrupt();
    // Force watchdog reset for a clean restart
    WDTCTL = 1;
    while(1){ __no_operation(); }
}


// *************************************************************************************************
// @fn          reset_ucs_reset_handler
// @brief       Initialize reset handler for UCS.
// @param       none
// @return      none
// *************************************************************************************************
void reset_ucs_reset_handler(void)
{
  sUcsResetHandling.status = UCSRESET_MAN;
  sUcsResetHandling.counter = 0;
}



