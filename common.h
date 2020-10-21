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

#define array_length(arr) (isize)(sizeof(arr) / sizeof(arr[0]))

#define assert_implies(pred, cond) assert(!(pred) || (cond))
#define assert_unreachable() assert(!"Unreachable codepath hit")

#if !defined(ALWAYS) && !defined(NEVER)
#define ALWAYS(X) ((X) ? 1 : (assert(0), 0))
#define NEVER(X) ((X) ? (assert(0), 1) : 0)
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4116) // unnamed type definition in parentheses
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable : 4204) // nonstandard extension used: non-constant aggregate initializer
#pragma warning(disable : 4221) // nonstandard extension used: cannot be initialized using address of automatic variable
#pragma warning(disable : 4057) // 'initializing': 'char *' differs in indirection to slightly different base types from 'u8 *'
#pragma warning(disable : 4709) // comma operator within array index expression
#pragma warning(disable : 4127) // conditional expression is constant
#pragma warning(disable : 4210) // nonstandard extension used: function given file scope
#pragma warning(disable : 4996) // _CRT_SECURE_NO_WARNINGS
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wpointer-sign"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunknown-pragmas"

#ifdef __FAST_MATH__
#warning -ffast-math breaks perfect parsing of sm files!!
#endif
#endif
