#ifndef _CLIENT_TYPES_H_
#define _CLIENT_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef unsigned char ascii_char;
typedef unsigned char uchar;

// takes no argument and returns a value (int)
typedef u8 (*Func)(void);

// takes an argument and does not return a value
typedef void (*Action)(void *);

// takes an argument and returns a value (int)
typedef u8 (*delegate)(void *);

#ifdef __cplusplus
}
#endif

#endif