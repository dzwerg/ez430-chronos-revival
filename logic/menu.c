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
// Menu management functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"
#include <stddef.h>

// driver
#include "display.h"

// logic
#include "menu.h"
#include "user.h"
#include "clock.h"
#include "date.h"
#include "beattime.h"
#include "binwatch.h"
#include "moon.h"
#include "timezone.h"
#include "alarm.h"
#include "stopwatch.h"
#include "temperature.h"
#include "altitude.h"
#include "battery.h"
#include "countdowntimer.h"
#include "random.h"


// *************************************************************************************************
// Defines section
#define FUNCTION(function)  function


// *************************************************************************************************
// Global Variable section
const struct menu * ptrMenu_L1 = NULL;
const struct menu * ptrMenu_L2 = NULL;
u8 MenuBinEnabled = 1;
u8 MenuDiceEnabled = 1;
u8 MenuBatteryEnabled = 1;
u8 MenuMoonEnabled = 1;
u8 MenuBeatEnabled = 1;
u8 MenuAltitudeEnabled = 1;
u8 MenuTemperatureEnabled = 1;
u8 MenuTimezone2Enabled = 1;


// *************************************************************************************************
// Global Variable section

void display_nothing(u8 line, u8 update) {}


u8 update_nothing(void)
{
	return (0);
}
u8 update_time(void)
{
	return (display.flag.update_time);
}
u8 update_stopwatch(void)
{
	return (display.flag.update_stopwatch);
}
u8 update_date(void)
{
	return (display.flag.update_date);
}
u8 update_alarm(void)
{
	return (display.flag.update_alarm);
}
u8 update_temperature(void)
{
	return (display.flag.update_temperature);
}
u8 update_battery_voltage(void)
{
	return (display.flag.update_battery_voltage);
}
// *************************************************************************************************
// User navigation ( [____] = default menu item after reset )
//
//	LINE1: 	[Time] -> Alarm -> Temperature -> Altitude/Ambient pressure
//
//	LINE2: 	[Date] -> Stopwatch -> Countdowntimer -> Agility indicator -> Number storage -> Random number generator -> Battery
//
// BlueRobin heart-rate/speed functions removed from the GCC build.
// *************************************************************************************************

// Line1 - Time
const struct menu menu_L1_Time =
{
	FUNCTION(sx_time),			// direct function
	FUNCTION(mx_time),			// sub menu function
	FUNCTION(display_time),		// display function
	FUNCTION(update_time),		// new display data
	&menu_L1_Alarm,
};
// Line1 - Alarm
const struct menu menu_L1_Alarm =
{
	FUNCTION(sx_alarm),			// direct function
	FUNCTION(mx_alarm),			// sub menu function
	FUNCTION(display_alarm),	// display function
	FUNCTION(update_alarm),		// new display data
	&menu_L1_Temperature,
};
// Line1 - Temperature
const struct menu menu_L1_Temperature =
{
	FUNCTION(dummy),					// direct function
	FUNCTION(mx_temperature),			// sub menu function
	FUNCTION(display_temperature),		// display function
	FUNCTION(update_temperature),		// new display data
	&menu_L1_Altitude,
};
// Line1 - Altitude
const struct menu menu_L1_Altitude =
{
	FUNCTION(sx_altitude),				// direct function
	FUNCTION(mx_altitude),				// sub menu function
	FUNCTION(display_altitude),			// display function
	FUNCTION(update_time),				// new display data
	&menu_L1_Battery,
};
// Line1 - Battery voltage
const struct menu menu_L1_Battery =
{
	FUNCTION(dummy),
	FUNCTION(dummy),
	FUNCTION(display_battery_V),
	FUNCTION(update_battery_voltage),
	&menu_L1_Time,
};
//-----------------------------------------------------------------------------
// Line2 - Date
const struct menu menu_L2_Date =
{
	FUNCTION(sx_date),			// direct function
	FUNCTION(mx_date),			// sub menu function
	FUNCTION(display_date),		// display function
	FUNCTION(update_date),		// new display data
	&menu_L2_TimeZone2,
};
// Line2 - Swatch Beat Time
const struct menu menu_L2_BeatTime =
{
	FUNCTION(dummy),			// direct function
	FUNCTION(dummy),			// sub menu function
	FUNCTION(display_beattime),	// display function
	FUNCTION(update_time),		// refresh once per second
	&menu_L2_BinWatch,
};
// Line2 - Binary full-screen watch face
const struct menu menu_L2_BinWatch =
{
	FUNCTION(dummy),
	FUNCTION(dummy),
	FUNCTION(display_binwatch),
	FUNCTION(update_time),
	&menu_L2_Moon,
};
// Line2 - Moon phase full-screen view
const struct menu menu_L2_Moon =
{
	FUNCTION(sx_moon),
	FUNCTION(dummy),
	FUNCTION(display_moon),
	FUNCTION(update_time),
	&menu_L2_Stopwatch,
};
// Line2 - Second Time Zone
const struct menu menu_L2_TimeZone2 =
{
	FUNCTION(sx_timezone2),
	FUNCTION(mx_timezone2),
	FUNCTION(display_timezone2),
	FUNCTION(update_time),
	&menu_L2_BeatTime,
};
// Line2 - Stopwatch
const struct menu menu_L2_Stopwatch =
{
	FUNCTION(sx_stopwatch),		// direct function
	FUNCTION(mx_stopwatch),		// sub menu function
	FUNCTION(display_stopwatch),// display function
	FUNCTION(update_stopwatch),	// new display data
	&menu_L2_cdtimer,
};
// Line2 - Countdowntimer
const struct menu menu_L2_cdtimer =
{
	FUNCTION(sx_cdtimer),		// direct function
	FUNCTION(mx_cdtimer),		// sub menu function
	FUNCTION(display_cdtimer),  // display function
	FUNCTION(update_time),	    // new display data
	&menu_L2_random,
};
// Line2 - Random generator
const struct menu menu_L2_random =
{
	FUNCTION(sx_random),		    // direct function
	FUNCTION(mx_random),	        // sub menu function
	FUNCTION(display_random),       // display function
	FUNCTION(update_time),	        // new display data
	&menu_L2_Date,
};
