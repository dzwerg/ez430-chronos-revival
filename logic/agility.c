// *************************************************************************************************
//
//  Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/ 
//   
//   
//    Redistribution and use in source and binary forms, with or without 
//    modification, are permitted provided that the following conditions 
//    are met:
//  
//      Redistributions of source code must retain the above copyright 
//      notice, this list of conditions and the following disclaimer.
//   
//      Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the 
//      documentation and/or other materials provided with the   
//      distribution.
//   
//      Neither the name of Texas Instruments Incorporated nor the names of
//      its contributors may be used to endorse or promote products derived
//      from this software without specific prior written permission.
//  
//    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *************************************************************************************************
// Agility measurement functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"
#include "stdlib.h"

// driver
#include "display.h"
#include "vti_as.h"
#include "ports.h"
#include "user.h"

// logic
#include "agility.h"
#include "acceleration.h"
#include "user.h"
#include "menu.h"
#include "display2.h"


// *************************************************************************************************
// Prototypes section
void start_agility(void);
void stop_agility(void);
void set_agility(void);
void write_agility_history(void);

// *************************************************************************************************
// Defines section
#define ACCEL_TIMEOUT_PREVENTION        5


// *************************************************************************************************
// Global Variable section
struct agil sAgil;


// *************************************************************************************************
// Extern section
extern u8 acceleration_value_is_positive(u8 value);
extern u16 convert_acceleration_value_to_mgrav(u8 value);
extern u8 acceleration_value_is_positive(u8 value);

// Global flag for proper acceleration sensor operation
extern u8 as_ok;



// *************************************************************************************************
// @fn          display_agility_indicator
// @brief       Display routine only for agility indicator.
// @param       u8 segments		    LCD segments where value is displayed
//				u32 value			Integer value to be displayed
//				u8 digits			Number of digits to convert
//				u8 blanks			Number of leadings blanks in itoa result string
// @return      none
// *************************************************************************************************
void display_agility_indicator(u8 segments, u32 value, u8 digits, u8 blanks)
{
	u8 * str;
    u32 DisplayValue;
    
    DisplayValue = value / sAgil.ScaleFactor;
    
    if(DisplayValue > 199999ul)
    {
        display_chars(segments, (u8*)"199999", SEG_ON_BLINK_ON);
    }
    else
    {
        // Display current agility measurement result
        str = chronos_itoa(DisplayValue, digits, blanks);
        display_chars(segments, str, SEG_ON_BLINK_OFF);
    }
}

// *************************************************************************************************
// @fn          agile_tick
// @brief       Called by 1Hz clock_tick function. 
// @param       none
// @return      none
// *************************************************************************************************
void agility_tick(void)
{
  // Make sure it is something running before we do anything else
  if (sAgil.state == AGIL_RUN)
  {
      u8 i;

      for(i = 0; i < 3; i++)
      {
          // Get data from sensor
          sAgil.xyz[i] = sAccel.xyz[i];  
          // Save previous data
          sAgil.PreviousData[i] = sAgil.CurrentData[i];
          // Calculate acceleration (result without sign information)
          sAgil.CurrentData[i] = convert_acceleration_value_to_mgrav(sAgil.xyz[i]) / 10;
          // If acceleraition is negative
          if (acceleration_value_is_positive(sAgil.xyz[i]) == 0) { sAgil.CurrentData[i] *= (-1); }
          // Accumulate (max. increment per sec is about 3 * 2000 / 10 / 10 = 60)
          sAgil.AgilityIndicator += ( abs(sAgil.PreviousData[i] - sAgil.CurrentData[i]) / 10);
      }
      
      // Decrease timeout counter
      if(sAgil.timeout > 1) 
      {
          sAgil.timeout--;
          // Check timeout of module acceleration.c
          if(sAccel.timeout < ACCEL_TIMEOUT_PREVENTION)
              { 
              // Set acceleration timeout counter to prevent an undesired stop
              sAccel.timeout = ACCEL_TIMEOUT_PREVENTION;
              }
      }
      // Timeout occured; Save current measurement to history; Stop measurement, if number of programmed cycles elapsed.
      else // sAgil.timeout == 1
      { 
          if(sAgil.cycles > 1)
          {
              sAgil.timeout = sAgil.TimeoutSetting;
              sAgil.cycles--;
          }
          else // sAgil.cycles == 1
          {
              sAgil.cycles = 0;
              stop_agility();
          }
      write_agility_history();
      sAgil.AgilityIndicator = 0;
      }
  }
}


// *************************************************************************************************
// @fn          reset_agility
// @brief       Reset global agility variables.
// @param       none
// @return      none
// *************************************************************************************************
void reset_agility(void)
{
    u8 i;

    // Default agility measurement is off
    sAgil.state         = AGIL_STOP;
    
    // Clear data
    for(i = 0; i < 3; i++)
    {
        sAgil.xyz[i] = sAgil.PreviousData[i] = sAgil.CurrentData[i] = 0;
    }
    sAgil.AgilityIndicator = 0;
    sAgil.ScaleFactor = AGIL_SCALE_DEFAULT;
    sAgil.HistoryIndex = 0;
    for(i = 0; i < AGIL_HISTORY_MAX; i++) { sAgil.History[i] = 0; }
    
    // Default settings for timeout
    sAgil.TimeoutSetting = AGIL_MEASUREMENT_TIMEOUT_DEFAULT;
    sAgil.timeout = sAgil.TimeoutSetting;
    
    // Default settings for cycles
    sAgil.CycleSetting = AGIL_MEASUREMENT_CYCLES_DEFAULT;
    sAgil.cycles = sAgil.CycleSetting;
}


// *************************************************************************************************
// @fn          start_agility
// @brief       Start agility measurement
// @param       none
// @return      none
// *************************************************************************************************
void start_agility(void)
{
   // Start sensor data acquisition if not already started by modul acceleration.c
   if(sAccel.mode == ACCEL_MODE_OFF)
   {
       // Start sensor incl. ISR
       as_start();
       // Set acceleration timeout counter to prevent an undesired stop from module timer.c
       if(sAccel.timeout < ACCEL_TIMEOUT_PREVENTION)
       {
           sAccel.timeout = ACCEL_TIMEOUT_PREVENTION;
       }
   }
   // Reset timeout and cycles if cycles elapsed
   if(sAgil.cycles == 0)
   {
       sAgil.timeout = sAgil.TimeoutSetting;
       sAgil.cycles = sAgil.CycleSetting;
   }
   // Set state to RUN
   sAgil.state = AGIL_RUN;
}


// *************************************************************************************************
// @fn          stop_agility
// @brief       Stop agility measurement
// @param       none
// @return      none
// *************************************************************************************************
void stop_agility(void)
{
   // Stop sensor data acquisition if not already used by modul acceleration.c
   if(sAccel.mode == ACCEL_MODE_OFF)
   {
       // Stop sensor
       as_stop();
   }
   // Set state to STOP
   sAgil.state = AGIL_STOP;

}


// *************************************************************************************************
// @fn          write_agility_history
// @brief       Write agility indicator to history
// @param       none
// @return      none
// *************************************************************************************************
void write_agility_history(void)
{
   sAgil.History[sAgil.HistoryIndex] = sAgil.AgilityIndicator;
   sAgil.HistoryIndex++;
   if(sAgil.HistoryIndex >= AGIL_HISTORY_MAX) { sAgil.HistoryIndex = 0; }
}



// *************************************************************************************************
// @fn          mx_agility
// @brief       Set routine for agility measurement.
// @param       u8 line LINE2
// @return      none
// *************************************************************************************************
void mx_agility(u8 line)
{
    u8 StateTmp;
    
    // Save state
    StateTmp = sAgil.state;
    
    // Stop measurement
    stop_agility();

    // Shows the history. Gives the opportunity to set the agility measurement parameter and clear history.
    set_agility();
    
    // Restore state
    if(StateTmp == AGIL_RUN) { start_agility(); }
    
    // Display update
    display_agility(line, DISPLAY_LINE_UPDATE_FULL);
}


// *************************************************************************************************
// @fn          sx_agility
// @brief       Agility measurement direct function. DOWN Button starts/stops Agility measurement.
// @param       u8 line     LINE2
// @return      none
// *************************************************************************************************
void sx_agility(u8 line)
{
    // DOWN Button: RUN, STOP
    if(button.flag.down)
    {
        if (sAgil.state == AGIL_STOP)
        {
            // Start Agility measurement
            start_agility();
        }
        else 
        {
            // Stop Agility measurement 
            stop_agility();
        }
    }
}



// *************************************************************************************************
// @fn          display_agility
// @brief       Display routine.
// @param       u8 line         LINE2
//              u8 update       DISPLAY_LINE_UPDATE_FULL, DISPLAY_LINE_CLEAR
// @return      none
// *************************************************************************************************
void display_agility(u8 line, u8 update)
{
    // Show warning if acceleration sensor was not initialised properly
    if (!as_ok)
    {
        clear_line(LINE2);
        display_chars(LCD_SEG_L2_2_0, (u8*)"ERR", SEG_ON);
    }
    else
    {
        // Redraw whole screen
        if((update == DISPLAY_LINE_UPDATE_FULL) || (update == DISPLAY_LINE_UPDATE_PARTIAL))
        {
            display_agility_indicator(LCD_SEG_L2_5_0, sAgil.AgilityIndicator, 6, 5);
        }
        else if (update == DISPLAY_LINE_CLEAR)
        {
            // Clean up display
            display_chars(LCD_SEG_L2_5_0, (u8*)"      ", SEG_OFF_BLINK_OFF);
            
        }
    }
}


// *************************************************************************************************
// @fn          set_agility
// @brief       Set's the agility measurement timeout. Shows the history
// @param       none
// @return      none
// *************************************************************************************************
void set_agility(void)
{
    u8  select;
    s32 TimeoutSettingTmp;
    s32 CycleSettingTmp;
    s32 HistoryTmp;
    s32 ResetTmp;
    s32 ScaleFactorTmp;
    u8  HistoryIndexTmp;
    u8  DisplayedHistoryIndex;
    u8 * str;
    u8 i;
        
    // Copy to temp
    ResetTmp = 0;
    TimeoutSettingTmp = sAgil.TimeoutSetting / SECONDS_OF_ONE_HOUR;
    CycleSettingTmp   = sAgil.CycleSetting;
    HistoryIndexTmp   = sAgil.HistoryIndex;
    DisplayedHistoryIndex = 1;
    ScaleFactorTmp    = sAgil.ScaleFactor;
    
    // Init value index
    select = 0; 
        
    // Loop values until all are set or user breaks set
    while(1) 
    {
        // Idle timeout: exit without saving 
        if (sys.flag.idle_timeout) break;
        
        // Immediatly take over ScaleFactor
        sAgil.ScaleFactor = ScaleFactorTmp;
        
        // NUM Button (short): save, then exit 
        if (button.flag.num) 
        {
            // Store local variables in global
            if(sAgil.TimeoutSetting != TimeoutSettingTmp * SECONDS_OF_ONE_HOUR)
            {
                sAgil.TimeoutSetting = TimeoutSettingTmp * SECONDS_OF_ONE_HOUR;
                sAgil.timeout = sAgil.TimeoutSetting;
                sAgil.AgilityIndicator = 0;
            }
            if(sAgil.CycleSetting != CycleSettingTmp)
            {
                sAgil.CycleSetting = CycleSettingTmp;
                sAgil.cycles = sAgil.CycleSetting;
            }
            //Set display update flag
            display.flag.full_update = 1;
            break;
        }

        switch (select)
        {
            case 0: // Display history
                    clear_display_all();
                    // Display a "H" for "History"
                    display_char(LCD_SEG_L1_3, 'H', SEG_ON);
                    
                    // Decrease field index
                    if(HistoryIndexTmp == 0) { HistoryIndexTmp = AGIL_HISTORY_MAX-1; }
                    else { HistoryIndexTmp--; }
                    
                    // Display the interval number as "-1" , "-2" ... "-10" and so on
                    str = chronos_itoa(DisplayedHistoryIndex, 3, 1);
                    display_chars(LCD_SEG_L1_2_0, str, SEG_ON);
                    if(DisplayedHistoryIndex < 10) { display_char(LCD_SEG_L1_1, '-', SEG_ON); }
                    else  { display_char(LCD_SEG_L1_2, '-', SEG_ON); }
                    
                    HistoryTmp = sAgil.History[HistoryIndexTmp];
                    set_value(&HistoryTmp, 6, 5, HistoryTmp, HistoryTmp, SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_5_0, display_agility_indicator);
                    
                    if(DisplayedHistoryIndex > AGIL_HISTORY_MAX-1) 
                    { 
                        DisplayedHistoryIndex = 1; 
                        select = 1;
                    }
                    else { DisplayedHistoryIndex++; }
                    break;
            case 1: // Clear history
                    clear_display_all();
                    display_chars(LCD_SEG_L2_4_0, (u8 *)"CLEAR", SEG_ON);
                    set_value(&ResetTmp, 0, 0, 0, 1, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_0, display_OFF_ON);
                    if(ResetTmp == 1) 
                    { 
                        for(i = 0; i < AGIL_HISTORY_MAX; i++) { sAgil.History[i] = 0; }
                        sAgil.HistoryIndex = 0;
                        sAgil.timeout = sAgil.TimeoutSetting;
                        sAgil.cycles = sAgil.CycleSetting;
                        sAgil.AgilityIndicator = 0;
                        ResetTmp = 0;
                    }
                    select = 2;
                    break;
            case 2: // Set timeout in hours
                    clear_display_all();
                    display_chars(LCD_SEG_L1_3_0, (u8 *)"SET ", SEG_ON);
                    display_char(LCD_SEG_L2_3, 'T', SEG_ON);
                    set_value(&TimeoutSettingTmp, 2, 1, 1, 24, SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_1_0, display_value1);
                    select = 3;
                    break;
            case 3: // Set number of cycles
                    display_char(LCD_SEG_L2_3, 'C', SEG_ON);
                    set_value(&CycleSettingTmp, 2, 1, 1, AGIL_HISTORY_MAX, SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_1_0, display_value1);
                    select = 4;
                    break;
            case 4: // Set scale factor
                    display_chars(LCD_SEG_L1_3_0, (u8 *)"SCAL", SEG_ON);
                    set_value(&ScaleFactorTmp, 5, 4, 1, 10000, SETVALUE_DISPLAY_VALUE + SETVALUE_FAST_MODE + SETVALUE_ROLLOVER_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_4_0, display_value1);
                    select = 0;
                    break;
        }
    }
    // Clear button flag
    button.all_flags = 0;
    
}

