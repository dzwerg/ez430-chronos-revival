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
// Countdown timer functions. It's similar to stopwatch functions.
// *************************************************************************************************

#ifndef cdtimer_H_
#define cdtimer_H_

// *************************************************************************************************
// Include section
#include <project.h>


// *************************************************************************************************
// Prototypes section
extern void start_cdtimer(void);
extern void stop_cdtimer(void);
extern void reset_cdtimer(void);
extern void cdtimer_tick(void);
extern void mx_cdtimer(u8 line);
extern void sx_cdtimer(u8 line);
extern void display_cdtimer(u8 line, u8 update);
extern void set_cdtimer(void);


// *************************************************************************************************
// Defines section
#define CDTIMER_STOP            (0u)
#define CDTIMER_RUN             (1u)
#define CDTIMER_HIDE            (2u)

#define CDTIMER_MODE_CONTINUE   (0u)
#define CDTIMER_MODE_ONCE       (1u)


// *************************************************************************************************
// Global Variable section
struct cdtimer
{
    u8  state;
    u8  mode;
    u8  drawFlag;
    
    //  time[0]     hour H
    //  time[1]     hour L
    //  time[2]     minute H
    //  time[3]     minute L
    //  time[4]     second H
    //  time[5]     second L
    u8  time[6];
    
    // Default cdtimer time
    u8  defaultTime[6];
        
};
extern struct cdtimer scdtimer;


// *************************************************************************************************
// Extern section


#endif
