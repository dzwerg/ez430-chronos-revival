#include "project.h"
#include "display.h"
#include "display2.h"
#include "ports.h"
#include "clock.h"
#include "date.h"
#include "user.h"
#include "beattime.h"
#include "timezone.h"

/* UTC offsets are stored in half-hours, but displayed as real hours.
   +2 = UTC+1, +11 = UTC+5.5, -6 = UTC-3. */
s8 PrimaryUtcOffsetHalfHours = 2;   /* UTC+1 default */
s8 SecondUtcOffsetHalfHours = 16;   /* UTC+8 default */

/* DST is a per-zone option. Main and TZ2 retain independent settings
   so a SWAP can exchange the complete timezone roles. */
u8 TimeZoneDstEnabled = 0;
u8 SecondTimeZoneDstEnabled = 0;
/* 0 = TZ2, 1 = Beats, 2 = main time with seconds. */
static u8 timezone2_view = 0;

static void display_raw_timezone_l2(u8 segment, u8 bits)
{
    u8 mirrored = (u8)(((bits << 4) & 0xF0u) | ((bits >> 4) & 0x0Fu));
    write_lcd_mem((u8 *)segments_lcdmem[segment], mirrored,
                  segments_bitmask[segment], SEG_ON);
}

static void display_swap_word(void)
{
    /* S + a wide two-digit W + A + P. */
    display_char(LCD_SEG_L2_4, 'S', SEG_ON);
    display_raw_timezone_l2(LCD_SEG_L2_3, SEG_C | SEG_D | SEG_E | SEG_F);
    display_raw_timezone_l2(LCD_SEG_L2_2, SEG_B | SEG_C | SEG_D | SEG_E);
    display_char(LCD_SEG_L2_1, 'A', SEG_ON);
    display_char(LCD_SEG_L2_0, 'P', SEG_ON);
}

static void display_timezone2_word(void)
{
    display_char(LCD_SEG_L2_4, 'T', SEG_ON);
    /* A 7-segment Z normally looks identical to 2.  Omit its centre bar. */
    display_raw_timezone_l2(LCD_SEG_L2_3, SEG_A | SEG_B | SEG_D | SEG_E);
    display_char(LCD_SEG_L2_2, '2', SEG_ON);
}

static void restore_primary_am_pm(void)
{
    if (sys.flag.use_metric_units)
    {
        display_symbol(LCD_SYMB_AM, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SYMB_PM, SEG_OFF_BLINK_OFF);
    }
    else
    {
        display_am_pm_symbol(sTime.hour);
    }
}

extern u8 DST_AutoFlag;

static s16 normalize_minutes(s16 m, s8 *day_delta)
{
    s8 d = 0;
    while (m < 0)
    {
        m += 1440;
        d--;
    }
    while (m >= 1440)
    {
        m -= 1440;
        d++;
    }
    if (day_delta) *day_delta = d;
    return m;
}

static u8 days_in_month(u8 month, u16 year)
{
    switch (month)
    {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2:
            return (u8)(((year % 4u) == 0u && (((year % 100u) != 0u) || ((year % 400u) == 0u))) ? 29u : 28u);
        default: return 30;
    }
}

static void shift_date_fields(u8 *day, u8 *month, u16 *year, u8 *dow, s8 delta)
{
    while (delta > 0)
    {
        (*day)++;
        *dow = (*dow >= 6u) ? 0u : (u8)(*dow + 1u);
        if (*day > days_in_month(*month, *year))
        {
            *day = 1;
            (*month)++;
            if (*month > 12u)
            {
                *month = 1;
                (*year)++;
            }
        }
        delta--;
    }

    while (delta < 0)
    {
        *dow = (*dow == 0u) ? 6u : (u8)(*dow - 1u);
        if (*day > 1u)
        {
            (*day)--;
        }
        else
        {
            if (*month > 1u)
                (*month)--;
            else
            {
                *month = 12u;
                if (*year > 1u) (*year)--;
            }
            *day = days_in_month(*month, *year);
        }
        delta++;
    }
}

static u8 last_sunday(u8 day, u8 dow, u8 days)
{
    u8 dow_last = (u8)((dow + (days - day)) % 7u);
    return (u8)(days - dow_last);
}

/* EU-style DST rule used by the existing Chronos timezone logic.
   'hour' is the displayed local hour for the evaluated zone. */
static u8 dst_active_for(u8 enabled, u8 month, u8 day, u8 dow, u8 hour)
{
    u8 d;

    if (!enabled) return 0;
    if (month < 3u || month > 10u) return 0;
    if (month > 3u && month < 10u) return 1;

    if (month == 3u)
    {
        d = last_sunday(day, dow, 31u);
        if (day > d) return 1;
        if (day < d) return 0;
        return (hour >= 3u);
    }

    d = last_sunday(day, dow, 31u);
    if (day < d) return 1;
    if (day > d) return 0;
    return (hour < 3u);
}

/* EU DST evaluator when 'hour' is STANDARD local time derived as UTC+offset.
   EU switches at 01:00 UTC:
     CET standard time: 02:00 -> 03:00 CEST on the last Sunday in March
     CEST:              03:00 -> 02:00 CET  on the last Sunday in October
   Therefore, when evaluating the unshifted standard-local clock, DST is active
   from 02:00 on the March transition day and stops at 02:00 on the October
   transition day.  This is the correct evaluator for TZ2 calculations. */
static u8 dst_active_for_standard_local(u8 enabled, u8 month, u8 day, u8 dow, u8 hour)
{
    u8 d;

    if (!enabled) return 0;
    if (month < 3u || month > 10u) return 0;
    if (month > 3u && month < 10u) return 1;

    if (month == 3u)
    {
        d = last_sunday(day, dow, 31u);
        if (day > d) return 1;
        if (day < d) return 0;
        return (hour >= 2u);
    }

    d = last_sunday(day, dow, 31u);
    if (day < d) return 1;
    if (day > d) return 0;
    return (hour < 2u);
}


void timezone_set_dst_enabled(u8 enabled)
{
    TimeZoneDstEnabled = enabled ? 1u : 0u;
}

u8 timezone_is_dst_active(void)
{
    return dst_active_for(TimeZoneDstEnabled,
                          sDate.month, sDate.day, sDate.DayOfWeek, sTime.hour);
}

static s16 utc_minutes_with_day(s8 *day_delta)
{
    s16 m = (s16)sTime.hour * 60 + (s16)sTime.minute;

    m -= (s16)PrimaryUtcOffsetHalfHours * 30;
    if (timezone_is_dst_active()) m -= 60;

    return normalize_minutes(m, day_delta);
}

s16 timezone_get_utc_minutes(void)
{
    return utc_minutes_with_day((s8 *)0);
}

s16 timezone_get_bmt_minutes(void)
{
    return normalize_minutes((s16)(timezone_get_utc_minutes() + 60), (s8 *)0);
}

/* Calculate TZ2 from UTC and apply TZ2's own DST setting. The temporary
   calendar copy means DST is evaluated on the TZ2 date even when TZ2 lies
   on the previous/next day relative to Main. */
s16 timezone_get_second_minutes(void)
{
    s8 utc_day, zone_day;
    s16 utc = utc_minutes_with_day(&utc_day);
    s16 local;
    u8 day = sDate.day;
    u8 month = sDate.month;
    u16 year = sDate.year;
    u8 dow = sDate.DayOfWeek;

    local = normalize_minutes((s16)(utc + (s16)SecondUtcOffsetHalfHours * 30), &zone_day);
    shift_date_fields(&day, &month, &year, &dow, (s8)(utc_day + zone_day));

    if (dst_active_for_standard_local(SecondTimeZoneDstEnabled, month, day, dow, (u8)(local / 60)))
        local = normalize_minutes((s16)(local + 60), (s8 *)0);

    return local;
}

/* Exchange the complete Main/TZ2 roles while preserving the same instant.
   The current Main local time/date is first converted to UTC, then rebuilt
   in the old TZ2 zone. Date is shifted if the new local time crosses
   midnight. */
void timezone_swap_main_second(void)
{
    s8 old_primary = PrimaryUtcOffsetHalfHours;
    s8 old_second = SecondUtcOffsetHalfHours;
    u8 old_main_dst = TimeZoneDstEnabled ? 1u : 0u;
    u8 old_second_dst = SecondTimeZoneDstEnabled ? 1u : 0u;
    s8 utc_day, std_day, total_shift;
    s16 utc, local;
    u8 target_dst;

    utc = utc_minutes_with_day(&utc_day);
    local = normalize_minutes((s16)(utc + (s16)old_second * 30), &std_day);
    total_shift = (s8)(utc_day + std_day);

    while (total_shift > 0)
    {
        add_day();
        total_shift--;
    }
    while (total_shift < 0)
    {
        sub_day();
        total_shift++;
    }

    target_dst = dst_active_for_standard_local(old_second_dst,
                                               sDate.month, sDate.day, sDate.DayOfWeek,
                                               (u8)(local / 60));
    if (target_dst)
    {
        s8 dst_day;
        local = normalize_minutes((s16)(local + 60), &dst_day);
        if (dst_day > 0) add_day();
        else if (dst_day < 0) sub_day();
    }

    sTime.hour = (u8)(local / 60);
    sTime.minute = (u8)(local % 60);
    /* Seconds deliberately remain unchanged: SWAP changes timezone, not instant. */
    sTime.drawFlag = 3;

    PrimaryUtcOffsetHalfHours = old_second;
    SecondUtcOffsetHalfHours = old_primary;

    TimeZoneDstEnabled = old_second_dst;
    DST_AutoFlag = old_second_dst;
    SecondTimeZoneDstEnabled = old_main_dst;

    display.flag.full_update = 1;
}

void display_timezone_offset(u8 segments, u32 value, u8 digits, u8 blanks)
{
    s32 sv = (s32)value;
    u16 halfhours;
    u16 hours;
    (void)segments;
    (void)digits;
    (void)blanks;

    halfhours = (u16)((sv < 0) ? -sv : sv);
    hours = halfhours / 2u;

    clear_line(LINE1);

    if (halfhours & 1u)
    {
        u16 t = (u16)(hours * 10u + 5u);
        display_chars(LCD_SEG_L1_3_1, chronos_itoa(t, 3, 1), SEG_ON);
        display_symbol(LCD_SEG_L1_DP1, SEG_ON);
    }
    else
    {
        display_chars(LCD_SEG_L1_1_0, chronos_itoa(hours, 2, 1), SEG_ON);
        display_symbol(LCD_SEG_L1_DP1, SEG_OFF);
    }
}

void display_timezone2(u8 line, u8 update)
{
    s16 m;
    u8 hh, mm;
    u8 t[5];
    (void)line;

    if (update == DISPLAY_LINE_CLEAR)
    {
        display_symbol(LCD_SEG_L2_COL1, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SEG_L2_COL0, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SEG_L2_DP, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_ICON_HEART, SEG_OFF_BLINK_OFF);
        restore_primary_am_pm();
        return;
    }
    if (update != DISPLAY_LINE_UPDATE_FULL &&
        update != DISPLAY_LINE_UPDATE_PARTIAL) return;

    if (timezone2_view == 1u)
    {
        u32 beats = get_swatch_beats_x100();
        u8 beat_text[6];
        beat_text[0] = (u8)('0' + (beats / 10000UL) % 10UL);
        beat_text[1] = (u8)('0' + (beats / 1000UL) % 10UL);
        beat_text[2] = (u8)('0' + (beats / 100UL) % 10UL);
        beat_text[3] = (u8)('0' + (beats / 10UL) % 10UL);
        beat_text[4] = (u8)('0' + beats % 10UL);
        beat_text[5] = 0;

        display_symbol(LCD_SEG_L2_COL1, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SEG_L2_COL0, SEG_OFF_BLINK_OFF);
        display_chars(LCD_SEG_L2_4_0, beat_text, SEG_ON);
        display_symbol(LCD_SEG_L2_DP, SEG_ON);
        display_symbol(LCD_ICON_HEART, SEG_ON);
        return;
    }

    if (timezone2_view == 2u)
    {
        u8 main_time[7];
        u8 hour12 = (u8)(sTime.hour % 12u);
        if (hour12 == 0u) hour12 = 12u;
        main_time[0] = (u8)('0' + hour12 / 10u);
        main_time[1] = (u8)('0' + hour12 % 10u);
        main_time[2] = (u8)('0' + sTime.minute / 10u);
        main_time[3] = (u8)('0' + sTime.minute % 10u);
        main_time[4] = (u8)('0' + sTime.second / 10u);
        main_time[5] = (u8)('0' + sTime.second % 10u);
        main_time[6] = 0;

        display_symbol(LCD_SEG_L2_DP, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_ICON_HEART, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SYMB_AM, SEG_OFF_BLINK_OFF);
        display_symbol(LCD_SYMB_PM,
                       (sTime.hour >= 12u) ? SEG_ON_BLINK_OFF : SEG_OFF_BLINK_OFF);
        display_chars(LCD_SEG_L2_5_0, main_time, SEG_ON);
        display_symbol(LCD_SEG_L2_COL1, SEG_ON_BLINK_OFF);
        display_symbol(LCD_SEG_L2_COL0, SEG_ON_BLINK_OFF);
        return;
    }

    display_symbol(LCD_SEG_L2_DP, SEG_OFF_BLINK_OFF);
    display_symbol(LCD_ICON_HEART, SEG_OFF_BLINK_OFF);
    m = timezone_get_second_minutes();
    hh = (u8)(m / 60);
    mm = (u8)(m % 60);

    t[0] = (u8)('0' + hh / 10u);
    t[1] = (u8)('0' + hh % 10u);
    t[2] = (u8)('0' + mm / 10u);
    t[3] = (u8)('0' + mm % 10u);
    t[4] = 0;

    display_chars(LCD_SEG_L2_3_0, t, SEG_ON);
    display_symbol(LCD_SEG_L2_COL1, SEG_OFF);
    display_symbol(LCD_SEG_L2_COL0,
                   (sTime.second & 1u) ? SEG_ON_BLINK_OFF : SEG_OFF_BLINK_OFF);
}

void sx_timezone2(u8 line)
{
    (void)line;
    timezone2_view++;
    if (timezone2_view > 2u)
    {
        timezone2_view = 0u;
        restore_primary_am_pm();
    }
}

/* TZ2 setup sequence:
   1) SWAP NO/YES
   2) UTC offset
   3) DST ON/OFF
   STAR at any field exits/saves the values already edited.
   # advances to the next field. */
void mx_timezone2(u8 line)
{
    s32 offset = SecondUtcOffsetHalfHours;
    s32 dst = SecondTimeZoneDstEnabled;
    s32 swap = 0;
    (void)line;

    clear_display_all();
    display_swap_word();
    set_value(&swap, 0, 0, 0, 1,
              SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE +
              SETVALUE_NEXT_VALUE,
              LCD_SEG_L1_3_0, display_OFF_ON);
    if (swap)
    {
        timezone_swap_main_second();
        offset = SecondUtcOffsetHalfHours;
        dst = SecondTimeZoneDstEnabled;
    }
    if (button.flag.star) { button.all_flags = 0; return; }

    clear_display_all();
    display_timezone2_word();
    set_value(&offset, 3, 0, -24, 28,
              SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE +
              SETVALUE_DISPLAY_ARROWS + SETVALUE_NEXT_VALUE,
              LCD_SEG_L1_3_0, display_timezone_offset);
    SecondUtcOffsetHalfHours = (s8)offset;
    if (button.flag.star) { button.all_flags = 0; return; }

    clear_display_all();
    display_chars(LCD_SEG_L2_4_0, (u8 *)" DST ", SEG_ON);
    set_value(&dst, 0, 0, 0, 1,
              SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE +
              SETVALUE_NEXT_VALUE,
              LCD_SEG_L1_3_0, display_OFF_ON);
    SecondTimeZoneDstEnabled = (u8)dst;

    button.all_flags = 0;
}
