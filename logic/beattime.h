#ifndef BEATTIME_H_
#define BEATTIME_H_
#include "project.h"
extern void display_beattime(u8 line, u8 update);
extern u16 get_swatch_beats(void);
extern u32 get_swatch_beats_x100(void);
#endif
