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
// Additional Basic display functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"

// driver
#include "display.h"

// logic
#include "display2.h"


// *************************************************************************************************
// Prototypes section


// *************************************************************************************************
// Defines section



// *************************************************************************************************
// Global Variable section

// Segment position:
//   A
// F   B
//   G
// E   C
//   D



// *************************************************************************************************
// @fn          display_charSegments
// @brief       Write to 7-segment characters.
// @param       u8 segment		    A valid LCD segment 
//				u8 charSegments		Segments of a character to display (SEG_A .. SEG_F)
//				u8 mode		        SEG_ON, SEG_OFF, SEG_BLINK
// @return      none
// *************************************************************************************************
void display_charSegments(u8 segment, u8 charSegments, u8 mode)
{
	u8 * lcdmem;			// Pointer to LCD memory
	u8 bitmask;			// Bitmask for character
	u8 bits, bits1;		// Bits to write
	
	// Write to single 7-segment character
	if ((segment >= LCD_SEG_L1_3) && (segment <= LCD_SEG_L2_DP))
	{
		// Get LCD memory address for segment from table
		lcdmem = (u8 *)segments_lcdmem[segment];

		// Get bitmask for character from table
		bitmask = segments_bitmask[segment];
		
		// Segments of a char to display
		bits = charSegments;

		// When addressing LINE2 7-segment characters need to swap high- and low-nibble,
		// because LCD COM/SEG assignment is mirrored against LINE1
		if (segment >= LCD_SEG_L2_5) 
		{
			bits1 = ((bits << 4) & 0xF0) | ((bits >> 4) & 0x0F);
			bits = bits1;

			// When addressing LCD_SEG_L2_5, need to convert ASCII '1' and 'L' to 1 bit,
			// because LCD COM/SEG assignment is special for this incomplete character
			if (segment == LCD_SEG_L2_5) 
			{
				if (charSegments == (SEG_B+SEG_C)) bits = BIT7;
				else  bits = 0;
			}
		}
		
		// Physically write to LCD memory		
		write_lcd_mem(lcdmem, bits, bitmask, mode);
	}
}	


// *************************************************************************************************
// @fn          display_OFF_ON
// @brief       Display routine for special text
// @param       u8 segments		LCD segments where value is displayed
//				u32 value	    see below
//				u8 digits		Number of digits to convert - not used
//				u8 blanks		Number of leadings blanks in itoa result string - not used
// @return      none
// *************************************************************************************************
void display_OFF_ON(u8 segments, u32 value, u8 digits, u8 blanks)
{
	if(value == 0) { display_chars(segments, (u8 *)"OFF ", SEG_ON_BLINK_OFF); }
	if(value == 1) { display_chars(segments, (u8 *)"ON  ", SEG_ON_BLINK_OFF); }
}


// *************************************************************************************************
// @fn          display_CONT_ONCE
// @brief       Display routine for special text
// @param       u8 segments		LCD segments where value is displayed - not used
//				u32 value	    see below
//				u8 digits		Number of digits to convert - not used
//				u8 blanks		Number of leadings blanks in itoa result string - not used
// @return      none
// *************************************************************************************************
void display_CONT_ONCE(u8 segments, u32 value, u8 digits, u8 blanks)
{
	if(value == 0) { display_chars(segments, (u8 *)"CONT", SEG_ON_BLINK_OFF); }
	if(value == 1) { display_chars(segments, (u8 *)"ONCE", SEG_ON_BLINK_OFF); }
}


// *************************************************************************************************
// @fn          display_NO_YES
// @brief       Display routine for special text
// @param       u8 segments		LCD segments where value is displayed - not used
//				u32 value	    see below
//				u8 digits		Number of digits to convert - not used
//				u8 blanks		Number of leadings blanks in itoa result string - not used
// @return      none
// *************************************************************************************************
void display_NO_YES(u8 segments, u32 value, u8 digits, u8 blanks)
{
	if(value == 0) { display_chars(segments, (u8 *)"NO  ", SEG_ON_BLINK_OFF); }
	if(value == 1) { display_chars(segments, (u8 *)"YES ", SEG_ON_BLINK_OFF); }
}


// *************************************************************************************************
// @fn          display_NO_YES_AUTO_MAN
// @brief       Display routine for special text
// @param       u8 segments		LCD segments where value is displayed - not used
//				u32 value	    see below
//				u8 digits		Number of digits to convert - not used
//				u8 blanks		Number of leadings blanks in itoa result string - not used
// @return      none
// *************************************************************************************************
void display_NO_YES_AUTO_MAN(u8 segments, u32 value, u8 digits, u8 blanks)
{
	if(value == 0) { display_chars(segments, (u8 *)"NO  ", SEG_ON_BLINK_OFF); }
	if(value == 1) { display_chars(segments, (u8 *)"YES ", SEG_ON_BLINK_OFF); }
	if(value == 2) { display_chars(segments, (u8 *)"AUTO", SEG_ON_BLINK_OFF); }
	if(value == 3) { display_chars(segments, (u8 *)"MAN ", SEG_ON_BLINK_OFF); }
}


// *************************************************************************************************
// @fn          display_SW_Version
// @brief       Display routine for special text
// @param       u8 segments		LCD segments where value is displayed - not used
//				u32 value	    see below
//				u8 digits		Number of digits to convert - not used
//				u8 blanks		Number of leadings blanks in itoa result string - not used
// @return      none
// *************************************************************************************************
void display_SW_Version(u8 segments, u32 value, u8 digits, u8 blanks)
{
	if(value == 0) { display_chars(segments, (u8 *)" 2 3", SEG_ON); }
	if(value == 1) { display_chars(segments, (u8 *)"2014", SEG_ON); }
	if(value == 2) { display_chars(segments, (u8 *)"0501", SEG_ON); }
}

