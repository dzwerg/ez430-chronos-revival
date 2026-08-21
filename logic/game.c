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
// Bubble game functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"
#include <stdlib.h>

// driver
#include "display.h"
#include "vti_as.h"

// logic
#include "acceleration.h"
#include "agility.h"
#include "user.h"
#include "buzzer.h"
#include "clock.h"
#include "display2.h"


// *************************************************************************************************
void DisplayBubbles(void);
void MovementControl(u16 x_data, u8 x_pos, u16 y_data, u8 y_pos);

// Bubble Positions
// L1_3 _2 _1 _0
//    1  3  5  7    top row
//    2  4  6  8    bottom row
#define FIELDS  8
// Movement number: up=1, down=2, right=3, left=4, 
// from bottom left to top right = 5
// from top righ to bottom left  = 6
// from top left to bottom right = 7
// from bottom right to top left = 8
// no movement = 0
// impossible movement = 9

static const u8 BubbleMovement[FIELDS][FIELDS] = {
    { 0, 2, 3, 7, 9, 9, 9, 9 },
    { 1, 0, 5, 3, 9, 9, 9, 9 },
    { 4, 6, 0, 2, 3, 7, 9, 9 },
    { 8, 4, 1, 0, 5, 3, 9, 9 },
    { 9, 9, 4, 6, 0, 2, 3, 7 },
    { 9, 9, 8, 4, 1, 0, 5, 3 },
    { 9, 9, 9, 9, 4, 6, 0, 2 },
    { 9, 9, 9, 9, 8, 4, 1, 0 } };
    
typedef enum {
    Start = 0,
    Running,
    Winning,
    DisplayResult,
    End
} BubbleGameState;

static struct {
    BubbleGameState GameState;
    u32 TimeCounter;
    u32 TimeCounterTmp;
    u16 GameTimeCounter;
    u8  SecondsTmp;
    u8  WaitTimeCounter;
    u8  MovementPositionsCovered[FIELDS];
    u8  CurrentBubblePosition;
    u8  OldBubblePosition;
} sBubbleGame;


// *************************************************************************************************
// @fn          BubbleGame
// @brief       Main control function for game.
// @param       x_data, y_data: acceleration in X and Y direction (0..114)
// @param       x_pos
// @return      none
// *************************************************************************************************
void ResetBubbleGame(void)
{
    sBubbleGame.GameState = Start;
}


// *************************************************************************************************
// @fn          BubbleGame
// @brief       Main control function for game.
// @param       x_data, y_data: acceleration in X and Y direction (0..114)
// @param       x_pos
// @return      none
// *************************************************************************************************
void BubbleGame(u16 x_data, u8 x_pos, u16 y_data, u8 y_pos)
{
    u8 i, Result, *str;
    
    switch(sBubbleGame.GameState) {
        case Start:
            sBubbleGame.TimeCounter = sBubbleGame.TimeCounterTmp = 0;
            sBubbleGame.GameTimeCounter = 0;
            sBubbleGame.SecondsTmp = sTime.second;
            for(i = 0; i < FIELDS; i++) { sBubbleGame.MovementPositionsCovered[i] = 0; }
            sBubbleGame.MovementPositionsCovered[rand() % FIELDS] = 1;
            sBubbleGame.CurrentBubblePosition = sBubbleGame.OldBubblePosition = rand() % FIELDS;
            sBubbleGame.MovementPositionsCovered[sBubbleGame.CurrentBubblePosition] = 1;
            DisplayBubbles();
            sBubbleGame.GameState++;
            break;
            
        case Running:
            MovementControl(x_data, x_pos, y_data, y_pos);
            if(sBubbleGame.CurrentBubblePosition != sBubbleGame.OldBubblePosition) 
            { 
                sBubbleGame.OldBubblePosition = sBubbleGame.CurrentBubblePosition;
                if(sBubbleGame.MovementPositionsCovered[sBubbleGame.CurrentBubblePosition] == 0)
                    { sBubbleGame.MovementPositionsCovered[sBubbleGame.CurrentBubblePosition] = 1; }
                else { sBubbleGame.MovementPositionsCovered[sBubbleGame.CurrentBubblePosition] = 0; }
                DisplayBubbles();
            }
            Result = 0;
            for(i = 0; i < FIELDS; i++) { Result += sBubbleGame.MovementPositionsCovered[i]; }
            if(Result >= FIELDS) 
            {
                sBubbleGame.WaitTimeCounter = sBubbleGame.GameTimeCounter;
                display_chars(LCD_SEG_L1_3_0, (u8 *)"8888", SEG_ON_BLINK_ON);
                start_buzzer(3, CONV_MS_TO_TICKS(50), CONV_MS_TO_TICKS(150));
                sBubbleGame.GameState++;
            }
            break;
        
        case Winning:
            if(sBubbleGame.GameTimeCounter > (sBubbleGame.WaitTimeCounter + 2)) 
            { 
                display_chars(LCD_SEG_L1_3_0, (u8 *)"   ", SEG_ON_BLINK_OFF);
                sBubbleGame.GameState++; 
            }
            break;
        
        case DisplayResult: 
            // Display result time
            str = chronos_itoa((u32)sBubbleGame.GameTimeCounter, 4, 3);
            display_chars(LCD_SEG_L1_3_0, str, SEG_ON);
            
            sBubbleGame.WaitTimeCounter = sBubbleGame.GameTimeCounter;
            sBubbleGame.GameState++;
            break;
        
        case End:
            // Display result time about 3 seconds, than restart game
            if(sBubbleGame.GameTimeCounter > (sBubbleGame.WaitTimeCounter + 3)) { sBubbleGame.GameState++; }
            break;
        
        default: 
            sBubbleGame.GameState = Start;
            break;
        }
    // Counts the times called
    sBubbleGame.TimeCounter++;
    // Counts the seconds
    if(sTime.second != sBubbleGame.SecondsTmp) 
    { 
        sBubbleGame.SecondsTmp = sTime.second;
        sBubbleGame.GameTimeCounter++; 
    }
    return;
    
}
//              heavy    easy
#define ACC_MIN    10   // 20
#define ACC_LOW    40   // 30
#define ACC_HIGH   70   // 80
#define TIMEOUT    30
typedef enum {
    WaitingForMovement = 0,
    WaitingForKeeping
} MovementDetectionState;


// *************************************************************************************************
// @fn          MovementControl
// @brief       Detects specific movements (shaking) in X and Y direction.
// @param       none
// @return      none
// *************************************************************************************************
void MovementControl(u16 x_data, u8 x_pos, u16 y_data, u8 y_pos)
{
    u8 i, DetectedMovement;
    static u8 Timeout;
    static MovementDetectionState MovementState;
    
    DetectedMovement = 0;
    
    switch(MovementState) {
        case WaitingForMovement:
            // x > 0 and y == 0 -> up=1  // x < 0 and y == 0 -> down=2
            if((x_data > ACC_LOW) && (x_data < ACC_HIGH) && (y_data < ACC_MIN))
            { 
                if(x_pos == 0) { DetectedMovement = 2; }
                else { DetectedMovement = 1; }
             }
            // x == 0 and y < 0 -> right=3 // x == 0 and y > 0 -> left=4
            else if((x_data < ACC_MIN) && (y_data > ACC_LOW) && (y_data < ACC_HIGH))
            { 
                if(y_pos == 0) { DetectedMovement = 3; }
                else { DetectedMovement = 4; }
            }
            else if((x_data > ACC_LOW/2) && (x_data < ACC_HIGH/2) && (y_data > ACC_LOW/2) && (y_data < ACC_HIGH/2))
            { 
                // x > 0 and y < 0 -> from bottom left to top right = 5
                if((x_pos != 0) && (y_pos == 0)) { DetectedMovement = 5; }
                // x < 0 and y > 0 -> from top righ to bottom left  = 6
                else if((x_pos == 0) && (y_pos != 0)) { DetectedMovement = 6; }
                // x < 0 and y < 0 -> from top left to bottom right = 7
                else if((x_pos == 0) && (y_pos == 0)) { DetectedMovement = 7; }
                // x > 0 and y > 0 -> from bottom right to top left = 8
                else if((x_pos != 0) && (y_pos != 0)) { DetectedMovement = 8; }
            }
            if(DetectedMovement != 0) 
            { 
                Timeout = TIMEOUT;
                // Calculate the new position
                for(i = 0; i < FIELDS; i++)
                {
                    if(BubbleMovement[sBubbleGame.OldBubblePosition][i] == DetectedMovement)
                    { 
                        sBubbleGame.CurrentBubblePosition = i; 
                        MovementState++;
                    }
                }
                
            }
            break;
        case WaitingForKeeping:
            // x and y about 0 -> no movement
            if((x_data < ACC_MIN) && (y_data < ACC_MIN))
            { 
                Timeout--;
                if(Timeout == 0)
                {
                    MovementState++;
                }
            }
            else { Timeout = TIMEOUT; }
            break;
        default: 
            MovementState = WaitingForMovement;
            break;
        }
    return;
}


// *************************************************************************************************
// @fn          DisplayBubble
// @brief       Displays all marked bubbles.
// @param       none
// @return      none
// *************************************************************************************************
void DisplayBubbles(void)
{
    u8 i, segment, charSegmentsAll, charSegmentsTop, charSegmentsBottom;
    
    charSegmentsAll = SEG_A+SEG_B+SEG_C+SEG_D+SEG_E+SEG_F+SEG_G;
    charSegmentsTop = SEG_A+SEG_B+SEG_G+SEG_F;
    charSegmentsBottom = SEG_C+SEG_D+SEG_E+SEG_G;
    display_chars(LCD_SEG_L1_3_0, (u8 *)"    ", SEG_OFF);
    
    for(i = 0; i < FIELDS; i += 2)
    {
        if(i == 0) { segment = LCD_SEG_L1_3; }
        else if(i == 2) { segment = LCD_SEG_L1_2; }
        else if(i == 4) { segment = LCD_SEG_L1_1; }
        else { segment = LCD_SEG_L1_0; }
        if((sBubbleGame.MovementPositionsCovered[i] != 0) && (sBubbleGame.MovementPositionsCovered[i+1] != 0))
        {
            display_charSegments(segment, charSegmentsAll, SEG_ON);
        }
        else 
        {
            if(sBubbleGame.MovementPositionsCovered[i] != 0) { display_charSegments(segment, charSegmentsTop, SEG_ON); }
            if(sBubbleGame.MovementPositionsCovered[i+1] != 0) { display_charSegments(segment, charSegmentsBottom, SEG_ON); }
        }
     }
     return;
}


