/*
 * Portable basic types for eZ430-Chronos Revival.
 *
 * This file replaces the basic integer typedefs that legacy OpenChronos
 * inherited indirectly from the BlueRobin bm.h header. BlueRobin is not
 * part of this project.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef EZ430_CHRONOS_TYPES_H
#define EZ430_CHRONOS_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t  u16;
typedef int16_t   s16;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint64_t  u64;
typedef int64_t   s64;

typedef float     f32;
typedef double    f64;


#ifndef TRUE
#define TRUE  (1 == 1)
#endif

#ifndef FALSE
#define FALSE (0 == 1)
#endif

#ifndef BIT
#define BIT(x) (1uL << (x))
#endif


/* Legacy aliases still used by a few original source modules. */
typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;

#endif /* EZ430_CHRONOS_TYPES_H */
