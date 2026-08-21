#include "project.h"
#include "display.h"
#include "clock.h"
#include "menu.h"
#include "binwatch.h"

extern void (*fptr_lcd_function_line1)(u8 line, u8 update);

/*
 * Binary watch face for the eZ430-Chronos LCD.
 *
 * Line 1: four binary digits representing the hour in 12-hour format.
 *         1..12 therefore fits naturally in four bits.
 * Line 2: six binary digits representing minutes 0..59.
 * PM symbol: on for internal hours 12..23, off for 0..11.
 *
 * The watch face intentionally uses both LCD lines.  Leaving it with '#'
 * restores Line1 to the normal Time menu (handled on DISPLAY_LINE_CLEAR).
 */

static void make_binary(u8 value, u8 bits, u8 *out)
{
    u8 i;
    for (i = 0; i < bits; ++i)
    {
        u8 shift = (u8)(bits - 1u - i);
        out[i] = (value & (1u << shift)) ? '1' : '0';
    }
    out[bits] = 0;
}

void display_binwatch(u8 line, u8 update)
{
    u8 hour12;
    u8 hbits[5];
    u8 mbits[7];
    (void)line;

    if (update == DISPLAY_LINE_CLEAR)
    {
        /* BIN owns both lines.  Restore the normal Line1 watch face when
           Line2 advances to the next menu entry. */
        clear_line(LINE1);
        clear_line(LINE2);
        display_symbol(LCD_SYMB_PM, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SYMB_AM, SEG_OFF_BLINK_OFF);

        ptrMenu_L1 = &menu_L1_Time;
        fptr_lcd_function_line1 = ptrMenu_L1->display_function;
        display.flag.line1_full_update = 1;
        fptr_lcd_function_line1(LINE1, DISPLAY_LINE_UPDATE_FULL);
        return;
    }

    if (update != DISPLAY_LINE_UPDATE_FULL &&
        update != DISPLAY_LINE_UPDATE_PARTIAL)
        return;

    /* Convert internal 24-hour clock to 1..12 for the four binary bits. */
    hour12 = (u8)(sTime.hour % 12u);
    if (hour12 == 0u) hour12 = 12u;

    make_binary(hour12, 4u, hbits);
    make_binary(sTime.minute, 6u, mbits);

    /* BIN deliberately owns both character rows. */
    clear_line(LINE1);
    clear_line(LINE2);
    display_chars(LCD_SEG_L1_3_0, hbits, SEG_ON);
    display_chars(LCD_SEG_L2_5_0, mbits, SEG_ON);

    /* The requested afternoon indication is the existing PM icon. */
    display_symbol(LCD_SYMB_AM, SEG_OFF_BLINK_OFF);
    display_symbol(LCD_SYMB_PM,
                   (sTime.hour >= 12u) ? SEG_ON_BLINK_OFF : SEG_OFF_BLINK_OFF);
}
