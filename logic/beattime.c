#include "project.h"
#include "display.h"
#include "clock.h"
#include "timezone.h"
#include "beattime.h"

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
    u32 b;
    u8 text[6];
    (void)line;

    if (update == DISPLAY_LINE_CLEAR)
    {
        display_symbol(LCD_ICON_HEART, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SEG_L2_DP, SEG_OFF_BLINK_OFF);
        return;
    }

    if (update != DISPLAY_LINE_UPDATE_FULL &&
        update != DISPLAY_LINE_UPDATE_PARTIAL) return;

    b = get_swatch_beats_x100();
    text[0] = (u8)('0' + (b / 10000UL) % 10UL);
    text[1] = (u8)('0' + (b / 1000UL) % 10UL);
    text[2] = (u8)('0' + (b / 100UL) % 10UL);
    text[3] = (u8)('0' + (b / 10UL) % 10UL);
    text[4] = (u8)('0' + b % 10UL);
    text[5] = 0;

    display_symbol(LCD_ICON_HEART, SEG_ON);
    display_chars(LCD_SEG_L2_4_0, text, SEG_ON);
    /* Use the dedicated decimal-point segment for xxx.xx. */
    display_symbol(LCD_SEG_L2_DP, SEG_ON);
}
