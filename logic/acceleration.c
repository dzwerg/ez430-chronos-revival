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
// Temperature measurement functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"

// driver
#include "display.h"
#include "vti_as.h"

// logic
#include "acceleration.h"
#include "agility.h"
#include "user.h"
#include "game.h"
#include "display2.h"


// *************************************************************************************************
// Global Variable section
struct accel sAccel;

// Conversion values from data to mgrav taken from CMA3000-D0x datasheet (rev 0.4, table 4)
/* BMA250 +/-2g is 10-bit.  We use the signed MSB (bits 9..2), giving
   15.625 mg per displayed LSB.  Integer weights below approximate that scale. */
const u16 mgrav_per_bit[7] = { 16, 31, 63, 125, 250, 500, 1000 };

// *************************************************************************************************
// Extern section

// Global flag for proper acceleration sensor operation
extern u8 as_ok;


// *************************************************************************************************
// @fn          reset_acceleration
// @brief       Reset acceleration variables.
// @param       none
// @return      none
// *************************************************************************************************
void reset_acceleration(void)
{
    // Start with Y-axis display
    sAccel.view_style   = DISPLAY_ACCEL_Y;
    
    // Clear timeout counter
    sAccel.timeout      = 0;

    // Default mode is off
    sAccel.mode         = ACCEL_MODE_OFF;
}


// *************************************************************************************************
// @fn          sx_acceleration
// @brief       Acceleration direct function. Button UP switches between X/Y/Z/WATER_BUBBLE/BUBBLE_GAME display.
// @param       u8 line     LINE2
// @return      none
// *************************************************************************************************
void sx_acceleration(u8 line)
{
    if (++sAccel.view_style > 4) sAccel.view_style = 0;
    
    // Reset current acceleration value
    sAccel.data = 0;
    
    // Get data from sensor
    as_get_data(sAccel.xyz);
}


// *************************************************************************************************
// @fn          acceleration_value_is_positive
// @brief       Returns 1 if 2's complement number is positive
// @param       u8 value    2's complement number
// @return      u8          1 = number is positive, 0 = number is negavtive
// *************************************************************************************************
u8 acceleration_value_is_positive(u8 value)
{
    return ((value & BIT7) == 0);
}


// *************************************************************************************************
// @fn          convert_acceleration_value_to_mgrav
// @brief       Converts measured value to mgrav units
// @param       u8 value    g data from sensor 
// @return      u16         Acceleration (mgrav)
// *************************************************************************************************
u16 convert_acceleration_value_to_mgrav(u8 value)
{
    u16 result;
    u8 i;
    
    if (!acceleration_value_is_positive(value))
    {
        // Convert 2's complement negative number to positive number
        value = ~value;
        value += 1;
    }
    
    result = 0;
    for (i=0; i<7; i++)
    {
        result += ((value & (BIT(i)))>>i) * mgrav_per_bit[i];
    }
    
    return (result);
}



// *************************************************************************************************
// @fn          is_acceleration_measurement - also used by timer.c
// @brief       Returns 1 if acceleration is currently measured.
// @param       none
// @return      u8      1 = acceleration measurement ongoing
// *************************************************************************************************
u8 is_acceleration_measurement(void)
{
    return (((sAccel.mode == ACCEL_MODE_ON) && (sAccel.timeout > 0)) || (sAgil.state != AGIL_STOP));
}

         

// *************************************************************************************************
// @fn          do_acceleration_measurement
// @brief       Get sensor data and store in sAccel struct
// @param       none
// @return      none
// *************************************************************************************************
void do_acceleration_measurement(void)
{
    // Get data from sensor
    as_get_data(sAccel.xyz);
    
    // Set display update flag
    display.flag.update_acceleration = 1;
}


// *************************************************************************************************
// @fn          display_acceleration
// @brief       Display routine.
// @param       u8 line         LINE1
//              u8 update       DISPLAY_LINE_UPDATE_FULL, DISPLAY_LINE_CLEAR
// @return      none
// *************************************************************************************************
void display_acceleration(u8 line, u8 update)
{
    u8 * str;
    u8 raw_data, x_raw, y_raw, x_pos, y_pos;
    u16 accel_data, x_data, y_data;
    static u16  old_x_data, old_y_data;
    
    // Show warning if acceleration sensor was not initialised properly
    if (!as_ok)
    {
        display_chars(LCD_SEG_L1_2_0, (u8*)"ERR", SEG_ON);
    }
    else
    {
        // Redraw whole screen
        if (update == DISPLAY_LINE_UPDATE_FULL) 
        {
            {
                // Start acceleration sensor
                if (!is_acceleration_measurement()) 
                {
                    // Clear previous acceleration value
                    sAccel.data = 0;
                    
                    // Start sensor if not already started by module agility.c
                    if(sAgil.state == AGIL_STOP) 
                    { 
                        as_start(); 
                    }
                    
                }
                // Init acceleration measurement
                if(sAccel.mode == ACCEL_MODE_OFF)
                {
                    // Set timeout counter
                    sAccel.timeout = ACCEL_MEASUREMENT_TIMEOUT;
                    
                    // Set mode
                    sAccel.mode = ACCEL_MODE_ON;
                    
                    // Start with Y-axis values
                    sAccel.view_style = DISPLAY_ACCEL_Y;
                    
                }
            }
        }
        else if (update == DISPLAY_LINE_UPDATE_PARTIAL)
        {
            // Display water bubble
            if((sAccel.view_style == DISPLAY_WATER_BUBBLE) || (sAccel.view_style == DISPLAY_BUBBLE_GAME))
            {
                x_raw = sAccel.xyz[0];
                y_raw = sAccel.xyz[1];
                x_data = convert_acceleration_value_to_mgrav(x_raw) / 10;
                x_pos = acceleration_value_is_positive(x_raw);
                y_data = convert_acceleration_value_to_mgrav(y_raw) / 10;
                y_pos = acceleration_value_is_positive(y_raw);
                // Filter acceleration
                x_data = (u16)((x_data + (4U * old_x_data)) / 5U);
                y_data = (u16)((y_data + (4U * old_y_data)) / 5U);
                // Store average acceleration
                old_x_data = x_data;
                old_y_data = y_data;
                
                if(sAccel.view_style == DISPLAY_WATER_BUBBLE)
                {
                    // Clear display
                    display_chars(LCD_SEG_L1_2_0, (u8 *)"   ", SEG_OFF);
                    display_symbol(LCD_SYMB_ARROW_DOWN, SEG_OFF);
                    display_symbol(LCD_SYMB_ARROW_UP, SEG_OFF);
                    display_charSegments(LCD_SEG_L1_3, SEG_C+SEG_D+SEG_E+SEG_G, SEG_ON);
                    
                    // Display water bubble if x = 0 and y = 0
                    if((x_data == 0) && (y_data == 0))
                    { 
                        display_charSegments(LCD_SEG_L1_1, SEG_A+SEG_F+SEG_E+SEG_D, SEG_ON);
                        display_charSegments(LCD_SEG_L1_0, SEG_A+SEG_B+SEG_C+SEG_D, SEG_ON);
                    }
                    // Display segments if x > 0 and y == 0
                    else if((x_pos != 0) && (x_data != 0) && (y_data == 0))
                    { 
                        display_charSegments(LCD_SEG_L1_1, SEG_A+SEG_F+SEG_G, SEG_ON);
                        display_charSegments(LCD_SEG_L1_0, SEG_A+SEG_B+SEG_G, SEG_ON);
                    }
                    // Display segments if x < 0 and y == 0
                    else if((x_pos == 0) && (x_data != 0) && (y_data == 0))
                    { 
                        display_charSegments(LCD_SEG_L1_1, SEG_D+SEG_E+SEG_G, SEG_ON);
                        display_charSegments(LCD_SEG_L1_0, SEG_D+SEG_C+SEG_G, SEG_ON);
                    }
                    // Display segments if x == 0 and y > 0
                    else if((x_data == 0) && (y_pos != 0) && (y_data != 0))
                    { 
                        display_charSegments(LCD_SEG_L1_1, SEG_A+SEG_B+SEG_C+SEG_D+SEG_E+SEG_F, SEG_ON);
                    }
                    // Display segments if x == 0 and y < 0
                    else if((x_data == 0) && (y_pos == 0) && (y_data != 0))
                    { 
                        display_charSegments(LCD_SEG_L1_0, SEG_A+SEG_B+SEG_C+SEG_D+SEG_E+SEG_F, SEG_ON);
                    }
                    // Display segments if x > 0 and y > 0
                    else if((x_pos != 0) && (x_data != 0) && (y_pos != 0) && (y_data != 0))
                    { 
                        display_charSegments(LCD_SEG_L1_1, SEG_A+SEG_F+SEG_G+SEG_B, SEG_ON);
                    }
                    // Display segments if x > 0 and y < 0
                    else if((x_pos != 0) && (x_data != 0) && (y_pos == 0) && (y_data != 0))
                    { 
                        display_charSegments(LCD_SEG_L1_0, SEG_A+SEG_B+SEG_G+SEG_F, SEG_ON);
                    }
                    // Display segments if x < 0 and y > 0
                    else if((x_pos == 0) && (x_data != 0) && (y_pos != 0) && (y_data != 0))
                    { 
                        display_charSegments(LCD_SEG_L1_1, SEG_E+SEG_D+SEG_C+SEG_G, SEG_ON);
                    }
                    // Display segments if x < 0 and y < 0
                    else if((x_pos == 0) && (x_data != 0) && (y_pos == 0) && (y_data != 0))
                    { 
                        display_charSegments(LCD_SEG_L1_0, SEG_C+SEG_D+SEG_E+SEG_G, SEG_ON);
                    }
                }
                else if(sAccel.view_style == DISPLAY_BUBBLE_GAME)
                {
                    // Display bubble game
                    BubbleGame(x_data, x_pos, y_data, y_pos);
                }
            }
            
            // Display acceleration value of current axis
            else
            {
                ResetBubbleGame();
                // Convert X/Y/Z values to mg
                switch (sAccel.view_style)
                {
                    case DISPLAY_ACCEL_X:       raw_data = sAccel.xyz[0];
                                                display_char(LCD_SEG_L1_3, 'X', SEG_ON);
                                                break;
                    case DISPLAY_ACCEL_Y:       raw_data = sAccel.xyz[1];
                                                display_char(LCD_SEG_L1_3, 'Y', SEG_ON);
                                                break;
                    default:                    // DISPLAY_ACCEL_Z:
                                                raw_data = sAccel.xyz[2];
                                                display_char(LCD_SEG_L1_3, 'Z', SEG_ON);
                                                break;
                }
                accel_data = convert_acceleration_value_to_mgrav(raw_data) / 10;
                
                // Filter acceleration
                accel_data = (u16)((accel_data + (4U * sAccel.data)) / 5U);
                
                // Store average acceleration
                sAccel.data = accel_data;   
        
                // Display acceleration in x.xx format
                str = chronos_itoa(accel_data, 3, 0);
                display_chars(LCD_SEG_L1_2_0, str, SEG_ON);
                
                // Display decimal point
                display_symbol(LCD_SEG_L1_DP1, SEG_ON);
                
                // Display sign
                if (acceleration_value_is_positive(raw_data)) 
                {
                    display_symbol(LCD_SYMB_ARROW_UP, SEG_ON);
                    display_symbol(LCD_SYMB_ARROW_DOWN, SEG_OFF);
                }
                else 
                {
                    display_symbol(LCD_SYMB_ARROW_UP, SEG_OFF);
                    display_symbol(LCD_SYMB_ARROW_DOWN, SEG_ON);
                }
            }
        }
        else if (update == DISPLAY_LINE_CLEAR)
        {
            // Stop acceleration sensor if not used by module agility.c
            if(sAgil.state == AGIL_STOP) 
            {
                as_stop();
            }
    
            // Clear mode
            sAccel.mode = ACCEL_MODE_OFF;
            ResetBubbleGame();
            
            // Clean up display
            display_symbol(LCD_SEG_L1_DP1, SEG_OFF);
            display_symbol(LCD_SYMB_ARROW_UP, SEG_OFF);
            display_symbol(LCD_SYMB_ARROW_DOWN, SEG_OFF);   
        }
    }
}
