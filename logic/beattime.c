#include "project.h"
#include "display.h"
#include "clock.h"
#include "timezone.h"
#include "menu.h"
#include "beattime.h"

extern void (*fptr_lcd_function_line1)(u8 line, u8 update);

static void display_l2_underscore(u8 segment)
{
    u8 bits = SEG_D;
    u8 mirrored = (u8)(((bits << 4) & 0xF0u) | ((bits >> 4) & 0x0Fu));
    write_lcd_mem((u8 *)segments_lcdmem[segment], mirrored,
                  segments_bitmask[segment], SEG_ON);
}

/* Return Swatch Internet Time in hundredths of a beat: 0..99999 means
   @000.00 .. @999.99.  One beat = 86.4 seconds, based on BMT (UTC+1). */
u32 get_swatch_beats_x100(void)
{
    s16 m = timezone_get_bmt_minutes();
    u32 seconds = (u32)m * 60UL + (u32)sTime.second;
    /* 100000 / 86400 simplifies to 125 / 108.  This avoids the
       32-bit overflow of seconds * 100000 on MSP430 while preserving
       exact integer hundredths-of-a-beat scaling. */
    return (seconds * 125UL) / 108UL;
}

u16 get_swatch_beats(void)
{
    return (u16)(get_swatch_beats_x100() / 100UL);
}

void display_beattime(u8 line, u8 update)
{
    u16 b;
    (void)line;

    if (update == DISPLAY_LINE_CLEAR)
    {
        clear_line(LINE1);
        clear_line(LINE2);
        ptrMenu_L1 = &menu_L1_Time;
        fptr_lcd_function_line1 = ptrMenu_L1->display_function;
        display.flag.line1_full_update = 1;
        fptr_lcd_function_line1(LINE1, DISPLAY_LINE_UPDATE_FULL);
        return;
    }

    if (update != DISPLAY_LINE_UPDATE_FULL &&
        update != DISPLAY_LINE_UPDATE_PARTIAL) return;

    b = get_swatch_beats();
    clear_line(LINE1);
    clear_line(LINE2);
    display_chars(LCD_SEG_L1_2_0, chronos_itoa(b, 3, 0), SEG_ON);
    display_l2_underscore(LCD_SEG_L2_4);
    display_chars(LCD_SEG_L2_3_0, (u8 *)"BEAT", SEG_ON);
}
