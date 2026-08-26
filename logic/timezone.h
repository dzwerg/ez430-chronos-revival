#ifndef TIMEZONE_H_
#define TIMEZONE_H_
#include "project.h"

extern s8 PrimaryUtcOffsetHalfHours;
extern s8 SecondUtcOffsetHalfHours;
extern u8 TimeZoneDstEnabled;
extern u8 SecondTimeZoneDstEnabled;

extern void timezone_set_dst_enabled(u8 enabled);
extern u8 timezone_is_dst_active(void);
extern s16 timezone_get_utc_minutes(void);
extern s16 timezone_get_bmt_minutes(void);
extern s16 timezone_get_second_minutes(void);
extern void timezone_swap_main_second(void);
extern void display_timezone_offset(u8 segments, u32 value, u8 digits, u8 blanks);
extern void display_timezone2(u8 line, u8 update);
extern void sx_timezone2(u8 line);
extern void mx_timezone2(u8 line);

#endif
