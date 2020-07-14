#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _MSC_VER

#define alignof _Alignof

#define no_inline __declspec(noinline)

#if !defined(NDEBUG)
// force_inline is a debugging aid mostly
// So force msvc to emit em
#define force_inline __forceinline extern
#else
// But maybe we should let the compiler decide when optimisations are on
#define force_inline __forceinline
#endif

#else
#error ???
#endif // _MSC_VER

#include "basetypes.h"

#define array_length(arr) (sizeof(arr) / sizeof(arr[0]))

#define assert_implies(pred, cond) assert(!(pred) || (cond))
#define assert_unreachable() assert(!"Unreachable codepath hit")

// Really strike confidence in the reader at the top of the file.
static isize total_bytes_leaked = 0;

static b32 is_power_of_two_or_zero(i32 x)
{
    return (x & (x - 1)) == 0;
}

static i32 clamp_low(i32 a, i32 b)
{
    return (a > b) ? a : b;
}

static i32 clamp_high(i32 a, i32 b)
{
    return (a < b) ? a : b;
}

static i32 clamp(i32 a, i32 b, i32 t)
{
    return clamp_low(a, clamp_high(b, t));
}

static f64 clamp_lowd(f64 a, f64 b)
{
    return (a > b) ? a : b;
}

typedef struct Stack
{
    u8 *buf;
    u8 *ptr;
    u8 *end;
} Stack;

Stack stack_make(u8 *buf, isize buf_size)
{
    return (Stack) { buf, buf, buf + buf_size };
}

#define stack_alloc(stack, type, count) (type *)stack_alloc_(stack, sizeof(type), alignof(type), count)
void *stack_alloc_(Stack *stack, usize elem_size, usize alignment, isize count)
{
    assert(stack->buf);
    assert((alignment & (alignment - 1)) == 0);
    assert(count >= 0);
    assert(SIZE_MAX / elem_size > (usize)count);

    usize size = elem_size * count;
    usize alignment_mask = (usize)alignment - 1;
    usize ptr = (usize)stack->ptr;
    usize alignment_offset = (alignment - (ptr & alignment_mask)) & alignment_mask;

    if ((stack->ptr + alignment_offset + size) > stack->end) {
        printf("oom :(");
        exit(1);
    }

    stack->ptr += alignment_offset;
    void *result = stack->ptr;
    stack->ptr += size;
    return result;
}

#define stack_push(stack, type, ...) (type *)stack_push_(stack, sizeof(type), alignof(type), (type[1]){__VA_ARGS__})
void *stack_push_(Stack *stack, usize size, usize alignment, void *buf)
{
    u8 *result = stack_alloc_(stack, size, alignment, 1);
    memcpy(result, buf, size);
    return result;
}

Stack scratch = {0};
Stack permanent_memory = {0};
#define alloc_scratch(type, count) stack_alloc(&scratch, type, count)
#define alloc(type, count) stack_alloc(&permanent_memory, type, count)

typedef struct Buffer
{
    u8 *buf;
    isize len;
    isize cap;
} Buffer;

i32 buffer_printf(Buffer *buf, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    i32 len = vsnprintf(buf->buf + buf->len, 0, fmt, args);
    va_end(args);
    if (buf->len + len > buf->cap) {
        return 0;
    }
    va_start(args, fmt);
    buf->len += vsnprintf(buf->buf + buf->len, buf->cap - buf->len, fmt, args);
    va_end(args);
    assert(buf->len <= buf->cap);
    return len;
}

Buffer alloc_buffer(isize size)
{
    return (Buffer) { alloc(u8, size), 0, size };
}

Buffer read_file(char *path)
{
    FILE *f;
    i32 ok = fopen_s(&f, path, "rb");
    assert(ok == 0);

    fseek(f, 0, SEEK_END);
    usize filesize = ftell(f);
    rewind(f);

    Buffer result = {
        alloc(u8, filesize + 1),
        filesize + 1,
        filesize + 1
    };

    usize read = fread(result.buf, 1, filesize, f);
    assert(read == filesize);

    fclose(f);

    result.buf[read] = 0;

    return result;
}

isize write_file(char *path, Buffer buf)
{
    FILE *f;
    i32 ok = fopen_s(&f, path, "wb");
    assert(ok == 0);
    isize written = fwrite(buf.buf, 1, buf.len, f);
    fclose(f);
    return written;
}

typedef struct Buf
{
    Stack *alloc;
    isize len;
    isize cap;
    i32 leaked;
    i32 cookie;
} Buf;
static_assert(alignof(Buf) == sizeof(usize), "Buf depends on it aligning to word size (lazy)");
enum {
    BufCookie = 0x1DEAL
};

force_inline Buf *buf_hdr(void *buf) { Buf *hdr = buf ? (Buf *)buf - 1 : 0; assert_implies(hdr, hdr->cookie == BufCookie); return hdr; }
force_inline u8 *hdr_buf(Buf *hdr) { return (u8 *)(hdr + 1); }
force_inline isize buf_len(void *buf) { return ((buf) ? buf_hdr(buf)->len : 0); }
force_inline isize buf_cap(void *buf) { return ((buf) ? buf_hdr(buf)->cap : 0); }
force_inline void buf_clear(void *buf) { if (buf) buf_hdr(buf)->len = 0; }
force_inline b32 buf_fits(void *buf, isize n) { return (buf_cap(buf) - buf_len(buf)) >= n; }
#define buf_cap_bytes(buf) (buf_cap(buf) * sizeof((buf)[0]))
#define buf_end(buf) ((buf) + buf_len(buf))
#define buf_last(buf) buf_end(buf)[-1]
#define buf_fit(buf, n) ((buf) = buf_fit_(buf_hdr(buf), sizeof((buf)[0]), n))
#define buf_push(buf, ...) (buf_fits((buf), 1) ? 0 : buf_fit(buf, 1), \
                           (buf)[buf_len(buf)] = (__VA_ARGS__), \
                           &(buf)[buf_hdr(buf)->len++])

#define buf_use(buf, alloc) ((buf) = (buf) ? buf_change_allocator(buf_hdr(buf), alloc) : buf_stack_(alloc, sizeof((buf)[0])))
#define buf_make(...) buf_make_(buf_fit_(0, sizeof(__VA_ARGS__), 1), &(__VA_ARGS__), sizeof(__VA_ARGS__))

force_inline
void *buf_make_(void *dest, void *src, size_t size) {
    memcpy(dest, src, size);
    buf_hdr(dest)->len++;
    return dest;
}

void *buf_change_allocator(Buf *hdr, Stack *stack)
{
    (void)hdr, stack;
    assert_unreachable();
    return 0;
}

no_inline
void *buf_fit_(Buf *hdr, isize size, isize count)
{
    assert(size < INTPTR_MAX / count);

    if (hdr == 0) {
        hdr = alloc_scratch(Buf, 1);
        isize cap = max(8, count);
        alloc_scratch(u8, size * cap);
        hdr->alloc = &scratch;
        hdr->len = 0;
        hdr->cap = cap;
        hdr->leaked = 0;
    } else if (hdr->len + count > hdr->cap) {
        isize cap = hdr->cap * 2;

        if (hdr->alloc->ptr == hdr_buf(hdr) + size*hdr->cap) {
            stack_alloc(hdr->alloc, u8, size * (cap - hdr->cap));
            hdr->cap = cap;
        } else {
            Buf *new_hdr = stack_alloc(hdr->alloc, Buf, 1);
            stack_alloc(hdr->alloc, u8, size * cap);
            new_hdr->alloc = hdr->alloc;
            new_hdr->len = hdr->len;
            new_hdr->cap = cap;

            memcpy(hdr_buf(new_hdr), hdr_buf(hdr), hdr->len * size);

            // leak old header. todo: literally anything else
            if (hdr->alloc == &permanent_memory) {
                isize leaked = hdr->len * size;
                new_hdr->leaked += (i32)leaked;
                total_bytes_leaked += leaked;
            }

            hdr = new_hdr;
        }
    } else {
        assert_unreachable();
    }

    hdr->cookie = BufCookie;
    return hdr + 1;
}

#define buf_stack(type, stack) buf_stack_(stack, sizeof(type))
void *buf_stack_(Stack *stack, isize size)
{
    Buf *hdr = stack_alloc(stack, Buf, 1);
    isize cap = 8;
    stack_alloc(stack, u8, size * cap);
    hdr->alloc = stack;
    hdr->len = 0;
    hdr->cap = cap;
    hdr->cookie = BufCookie;
    return hdr + 1;
}

typedef struct String
{
    char *buf;
    isize len;
} String;
#define S(imm) ((String) { .buf = imm, .len = sizeof(imm) - 1 })
// msvc in its infinite wisdom needs this for static init
#define SS(imm) { .buf = imm, .len = sizeof(imm) - 1 }
