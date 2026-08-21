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

#ifndef AGILITY_H_
#define AGILITY_H_


// *************************************************************************************************
// Include section


// *************************************************************************************************
// Prototypes section



// *************************************************************************************************
// Defines section
#define AGIL_STOP            (0u)
#define AGIL_RUN             (1u)

// Default time to stop agility measurement to save battery
#define SECONDS_OF_ONE_HOUR                     (60ul*60ul)                  // seconds of 1 hour 
#define AGIL_MEASUREMENT_TIMEOUT_DEFAULT        (1ul*SECONDS_OF_ONE_HOUR)   // 1 hour

#define AGIL_MEASUREMENT_CYCLES_DEFAULT         1   // 1 cycle

#define AGIL_HISTORY_MAX        16

#define AGIL_SCALE_DEFAULT      10


// *************************************************************************************************
// Global Variable section
struct agil
{
    // AGIL_STOP, AGIL_RUN
    u8          state;

    // Sensor raw data
    u8          xyz[3];

    // Agility data
    s16         CurrentData[3];
    s16         PreviousData[3];
    
    // Agility indicator
    u32         AgilityIndicator;
    
    // Scale factor for displayed data
    u32         ScaleFactor;
    
    // Agility history
    u32         History[AGIL_HISTORY_MAX];
    u8          HistoryIndex;

    // Timeout in seconds for a measurement cycle
    u32         timeout;    
    u32         TimeoutSetting; 
    
    // Number of cycles
    u8          cycles;    
    u8          CycleSetting; 
};
extern struct agil sAgil;


// *************************************************************************************************
// Extern section
extern void reset_agility(void);
extern void mx_agility(u8 line);
extern void sx_agility(u8 line);
extern void display_agility(u8 line, u8 update);
extern void agility_tick(void);

#endif
