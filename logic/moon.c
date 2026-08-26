#include "project.h"
#include "display.h"
#include "clock.h"
#include "date.h"
#include "menu.h"
#include "timezone.h"
#include "moon.h"

#define SYNODIC_MINUTES 42524L
#define HALF_MOON_MINUTES 21262L
#define NEW_MOON_2000_MINUTES 8294L

extern void (*fptr_lcd_function_line1)(u8 line, u8 update);

static u8 moon_view;

static u8 leap_year(u16 year)
{
    return (u8)(((year % 4u) == 0u) &&
                (((year % 100u) != 0u) || ((year % 400u) == 0u)));
}

static s32 days_before_year(u16 year)
{
    s32 y = (s32)year - 1L;
    s32 base = 1999L;
    return 365L * (y - base) + (y / 4L - base / 4L) -
           (y / 100L - base / 100L) + (y / 400L - base / 400L);
}

static s32 moon_age_minutes(void)
{
    static const u16 month_start[12] =
        { 0,31,59,90,120,151,181,212,243,273,304,334 };
    s32 days;
    s32 utc_minutes;
    s32 age;
    u8 month = sDate.month;

    if (month < 1u || month > 12u) month = 1u;
    days = days_before_year(sDate.year) + month_start[month - 1u] +
           (s32)sDate.day - 1L;
    if (month > 2u && leap_year(sDate.year)) days++;

    utc_minutes = days * 1440L + (s32)sTime.hour * 60L + sTime.minute;
    utc_minutes -= (s32)PrimaryUtcOffsetHalfHours * 30L;
    if (timezone_is_dst_active()) utc_minutes -= 60L;

    age = (utc_minutes - NEW_MOON_2000_MINUTES) % SYNODIC_MINUTES;
    if (age < 0) age += SYNODIC_MINUTES;
    return age;
}

static u8 illuminated_percent(s32 age)
{
    u32 x;
    u32 other;
    u32 a;
    u32 b;

    if (age > HALF_MOON_MINUTES) age = SYNODIC_MINUTES - age;
    x = (u32)(age / 10L);
    other = 2126UL - x;
    a = x * x;
    b = other * other;
    if ((a + b) == 0UL) return 0;
    return (u8)((100UL * a + ((a + b) / 2UL)) / (a + b));
}

static void display_raw_l2(u8 segment, u8 bits)
{
    u8 mirrored = (u8)(((bits << 4) & 0xF0u) | ((bits >> 4) & 0x0Fu));
    write_lcd_mem((u8 *)segments_lcdmem[segment], mirrored,
                  segments_bitmask[segment], SEG_ON);
}

static void display_moon_word(void)
{
    /* Two-character M followed by O O N. */
    display_raw_l2(LCD_SEG_L2_4, SEG_A | SEG_B | SEG_E | SEG_F);
    display_raw_l2(LCD_SEG_L2_3, SEG_A | SEG_B | SEG_C | SEG_F);
    display_char(LCD_SEG_L2_2, 'O', SEG_ON);
    display_char(LCD_SEG_L2_1, 'O', SEG_ON);
    display_char(LCD_SEG_L2_0, 'N', SEG_ON);
}

static void display_new_word(void)
{
    /* N E followed by a two-character W (the inverted two-part M). */
    display_char(LCD_SEG_L2_3, 'N', SEG_ON);
    display_char(LCD_SEG_L2_2, 'E', SEG_ON);
    display_raw_l2(LCD_SEG_L2_1, SEG_C | SEG_D | SEG_E | SEG_F);
    display_raw_l2(LCD_SEG_L2_0, SEG_B | SEG_C | SEG_D | SEG_E);
}

static void display_days_to(s32 minutes)
{
    u16 tenths = (u16)((minutes + 72L) / 144L);
    display_chars(LCD_SEG_L1_3_1, chronos_itoa(tenths, 3, 0), SEG_ON);
    display_symbol(LCD_SEG_L1_DP1, SEG_ON);
}

void sx_moon(u8 line)
{
    (void)line;
    moon_view++;
    if (moon_view >= 3u) moon_view = 0u;
}

void display_moon(u8 line, u8 update)
{
    s32 age;
    s32 remaining;
    (void)line;

    if (update == DISPLAY_LINE_CLEAR)
    {
        moon_view = 0u;
        clear_line(LINE1);
        clear_line(LINE2);
        display_symbol(LCD_SYMB_ARROW_UP, SEG_OFF);
        display_symbol(LCD_SYMB_ARROW_DOWN, SEG_OFF);
        display_symbol(LCD_SEG_L1_DP1, SEG_OFF);
        ptrMenu_L1 = &menu_L1_Time;
        fptr_lcd_function_line1 = ptrMenu_L1->display_function;
        display.flag.line1_full_update = 1;
        fptr_lcd_function_line1(LINE1, DISPLAY_LINE_UPDATE_FULL);
        return;
    }

    if (update != DISPLAY_LINE_UPDATE_FULL && update != DISPLAY_LINE_UPDATE_PARTIAL)
        return;

    age = moon_age_minutes();
    clear_line(LINE1);
    clear_line(LINE2);
    display_symbol(LCD_SEG_L1_DP1, SEG_OFF);
    display_symbol(LCD_SYMB_ARROW_UP, SEG_OFF);
    display_symbol(LCD_SYMB_ARROW_DOWN, SEG_OFF);

    if (moon_view == 0u)
    {
        display_chars(LCD_SEG_L1_2_0,
                      chronos_itoa(illuminated_percent(age), 3, 0), SEG_ON);
        if (age > 0L && age < HALF_MOON_MINUTES)
            display_symbol(LCD_SYMB_ARROW_UP, SEG_ON);
        else if (age > HALF_MOON_MINUTES)
            display_symbol(LCD_SYMB_ARROW_DOWN, SEG_ON);
        display_moon_word();
    }
    else if (moon_view == 1u)
    {
        remaining = (age == 0L) ? 0L : SYNODIC_MINUTES - age;
        display_days_to(remaining);
        display_new_word();
    }
    else
    {
        remaining = HALF_MOON_MINUTES - age;
        if (remaining < 0L) remaining += SYNODIC_MINUTES;
        display_days_to(remaining);
        display_chars(LCD_SEG_L2_3_0, (u8 *)"FULL", SEG_ON);
    }
}
