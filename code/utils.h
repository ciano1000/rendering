#include <stdint.h>
#include <limits.h>

typedef int8_t s8;
typedef uint8_t u8;

typedef int16_t s16;
typedef uint16_t u16;

typedef int32_t s32;
typedef uint32_t u32;

typedef int64_t s64;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef u32 b32;
#define true 1
#define false 0
#define null 0

#define global static
#define internal static

#define Kilobytes(kb) (((u64)kb) << 10 )
#define Megabytes(mb) (((u64)mb) << 20 )
#define Gigabytes(gb) (((u64)gb) << 30 )
#define Terabytes(tb) (((u64)tb) << 40 )


#define ArrayCount(array) (sizeof(array)/sizeof(array[0]))
// TODO(Cian): intrinsics????
// TODO(Cian): Look at pulling these out into a Maths module later 

#define TOKEN_PASTE(x, y) x##y
#define CAT(x,y) TOKEN_PASTE(x,y)
#define UNIQUE_INT CAT(prefix, __COUNTER__)
#define _DEFER_LOOP(begin, end, var) for(int var = (begin,0); !var; ++var, end)

#define  OVERFLOW_ADD(a, b, res, type)  \
if(a > type##_MAX - b)  { \
res = type##_MAX; \
} else { \
res = a + b\
}\
