#include <stddef.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef u32 b32;
typedef u8 b8;

typedef size_t usize;
typedef intptr_t isize;

typedef float f32;
typedef double f64;

#define array_length(arr) (sizeof(arr) / sizeof(arr[0]))

#define assert_implies(pred, cond) assert(!(pred) || (cond))
#define assert_unreachable() assert(!"Unreachable codepath hit")

#if !defined(ALWAYS) && !defined(NEVER)
#define ALWAYS(X) ((X) ? 1 : (assert(0), 0))
#define NEVER(X) ((X) ? (assert(0), 1) : 0)
#endif
