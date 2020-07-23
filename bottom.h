// Buncha top-of-the-file crap that will just confuse anyone trying to look at
// main.c to see where the good stuff is.

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define alignof _Alignof

#ifdef _MSC_VER

#undef min
#undef max

#if !defined(NDEBUG)
// force_inline is a debugging aid mostly
// So force msvc to emit em
#define force_inline __forceinline extern
#else
#define force_inline
#endif

#else
#define force_inline
#endif // _MSC_VER

#include "stb_sprintf.h"
#undef snprintf
#define sprintf stbsp_sprintf
#define snprintf stbsp_snprintf
#define vsprintf stbsp_vsprintf
#define vsnprintf stbsp_vsnprintf

#include "common.h"

// Really strike confidence in the reader at the top of the file.
static isize total_bytes_leaked = 0;

static isize mins(isize a, isize b)
{
    return (a < b) ? a : b;
}

static isize maxs(isize a, isize b)
{
    return (a >= b) ? a : b;
}

static b32 is_power_of_two_or_zero(i32 x)
{
    return (x & (x - 1)) == 0;
}

static f32 clamp_low(f32 a, f32 b)
{
    return (a > b) ? a : b;
}

static f32 clamp_high(f32 a, f32 b)
{
    return (a <= b) ? a : b;
}

static f32 clamp(f32 a, f32 b, f32 t)
{
    return clamp_low(a, clamp_high(b , t));
}

static f32 lerp(f32 a, f32 b, f32 t)
{
    return a*(1.0f - t) + b*t;
}

static f32 min(f32 a, f32 b)
{
    return (a < b) ? a : b;
}

static f32 max(f32 a, f32 b)
{
    return (a >= b) ? a : b;
}

static f32 safe_div(f32 a, f32 b)
{
    if (b == 0.0f) {
        return 0.0f;
    }

    return a / b;
}

typedef struct Stack
{
    u8 *buf;
    u8 *ptr;
    u8 *end;
} Stack;

typedef struct StackMark
{
    Stack *stack;
    u8 *mark;
} StackMark;

Stack stack_make(u8 *buf, isize buf_size)
{
    return (Stack) { buf, buf, buf + buf_size };
}

StackMark stack_mark(Stack *stack)
{
    return (StackMark) { .stack = stack, .mark = stack->ptr };
}

void stack_release(StackMark mark)
{
    mark.stack->ptr = mark.mark;
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

Stack scratch_stack = {0};
Stack permanent_memory_stack = {0};
Stack *const scratch = &scratch_stack;
Stack *const permanent_memory = &permanent_memory_stack;
Stack *current_allocator = &permanent_memory_stack;
#define alloc_scratch(type, count) stack_alloc(scratch, type, count)
#define alloc(type, count) stack_alloc(current_allocator, type, count)

void reset_scratch()
{
#ifndef NDEBUG
    memset(scratch->ptr, 0x3c, scratch->ptr - scratch->buf);
#endif
    scratch->ptr = scratch->buf;
}

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
    buf->len += vsnprintf(buf->buf + buf->len, (i32)(buf->cap - buf->len), fmt, args);
    va_end(args);
    assert(buf->len <= buf->cap);
    return len;
}

Buffer alloc_buffer(isize size)
{
    return (Buffer) { alloc(u8, size), 0, size };
}

// This is for development stuff and so doesn't need to handle errors properly!!
Buffer read_file(const char *path)
{
#ifdef _MSC_VER
    FILE *f;
    fopen_s(&f, path, "rb");
#else
    FILE *f = fopen(path, "rb");
#endif
    assert(f);

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
    BufCookie = 0xAC00C1E4U
};

force_inline Buf *buf_hdr(void *buf) { Buf *hdr = buf ? (Buf *)buf - 1 : 0; assert_implies(hdr, hdr->cookie == BufCookie); return hdr; }
force_inline u8 *hdr_buf(Buf *hdr) { return (u8 *)(hdr + 1); }
force_inline isize buf_len(void *buf) { return ((buf) ? buf_hdr(buf)->len : 0); }
force_inline isize buf_cap(void *buf) { return ((buf) ? buf_hdr(buf)->cap : 0); }
force_inline void buf_clear(void *buf) { if (buf) buf_hdr(buf)->len = 0; }
force_inline b32 buf_fits(void *buf, isize n) { return (buf_cap(buf) - buf_len(buf)) >= n; }
#define buf_elem_size(buf) sizeof((buf)[0])
#define buf_cap_bytes(buf) (buf_cap(buf) * buf_elem_size(buf))
#define buf_end(buf) ((buf) + buf_len(buf))
#define buf_last(buf) buf_end(buf)[-1]
#define buf_fit(buf, n) ((buf) = buf_fit_(buf_hdr(buf), buf_elem_size(buf), n))
#define buf_maybe_fit(buf, n) (buf_fits((buf), (n)) ? 0 : buf_fit((buf), (n)))
#define buf_push(buf, ...) (buf_maybe_fit(buf, 1), \
                           (buf)[buf_len(buf)] = (__VA_ARGS__), \
                           &(buf)[buf_hdr(buf)->len++])
#define buf_pop(buf) (assert(buf_len(buf) > 0), (buf)[--buf_hdr(buf)->len])
#define buf_make(...) buf_make_(buf_fit_(0, sizeof(__VA_ARGS__), 1), &(__VA_ARGS__), sizeof(__VA_ARGS__))
#define buf_reserve(buf, n) (buf_fits((buf), (n) - buf_len(buf)) ? (buf) : buf_fit((buf), (n) - buf_len(buf)))

#define buf_pushn(buf, n) (buf_maybe_fit((buf), (n)), buf_pushn_((buf), buf_elem_size(buf), (n)))
force_inline
void *buf_pushn_(void *buf, isize size, isize count)
{
    void *result = (u8 *)buf + buf_len(buf) * size;
    buf_hdr(buf)->len += count;
    memset(result, 0, size * count);
    return result;
}

force_inline
void *buf_make_(void *dest, void *src, size_t size)
{
    memcpy(dest, src, size);
    buf_hdr(dest)->len++;
    return dest;
}

void *buf_fit_(Buf *hdr, isize size, isize count)
{
    assert(size < INTPTR_MAX / count);

    if (hdr == 0) {
        hdr = stack_alloc(current_allocator, Buf, 1);
        isize cap = maxs(8, count);
        stack_alloc(current_allocator, u8, size * cap);
        memset(hdr + 1, 0, size * cap);
        hdr->alloc = current_allocator;
        hdr->len = 0;
        hdr->cap = cap;
        hdr->leaked = 0;
    } else if (hdr->len + count > hdr->cap) {
        isize cap = hdr->cap * 2;

        if (hdr->alloc->ptr == hdr_buf(hdr) + size*hdr->cap) {
            stack_alloc(hdr->alloc, u8, size * (cap - hdr->cap));
            memset(hdr_buf(hdr) + size*hdr->cap, 0, size * (cap - hdr->cap));
            hdr->cap = cap;
        } else {
            Buf *new_hdr = stack_alloc(hdr->alloc, Buf, 1);
            stack_alloc(hdr->alloc, u8, size * cap);
            new_hdr->alloc = hdr->alloc;
            new_hdr->len = hdr->len;
            new_hdr->cap = cap;

            isize n = hdr->len * size;
            memcpy(hdr_buf(new_hdr), hdr_buf(hdr), n);
            memset(hdr_buf(new_hdr) + n, 0, size * cap - n);

            // leak old header. todo: literally anything else
            if (hdr->alloc == permanent_memory) {
                isize leaked = hdr->len * size;
                new_hdr->leaked += (i32)leaked;
                total_bytes_leaked += leaked;
            }

            hdr->cookie = 0;
            hdr = new_hdr;
        }
    } else {
        assert_unreachable();
    }

    hdr->cookie = BufCookie;
    return hdr + 1;
}

#define buf_printf(buf, ...) buf_printf_(&(buf), __VA_ARGS__)
i32 buf_printf_(char **buf_ref, char *fmt, ...)
{
    char *buf = *buf_ref;

    va_list args;
    va_start(args, fmt);
    i32 len = vsnprintf(0, 0, fmt, args);
    va_end(args);

    buf_reserve(buf, buf_len(buf) + len + 1);

    va_start(args, fmt);
    buf_hdr(buf)->len += vsnprintf(buf_end(buf), (i32)(buf_cap(buf) - buf_len(buf) + 1), fmt, args);
    va_end(args);
    assert(buf_len(buf) <= buf_cap(buf));

    *buf_ref = buf;
    return len;
}

#define buf_index_of(buf, elem) buf_index_of_(buf_hdr(buf), elem - buf)
force_inline
isize buf_index_of_(Buf *hdr, isize index)
{
    if (ALWAYS(hdr && index >= 0 && index <= hdr->len)) {
        return index;
    }

    return -1;
}

void buf_set_len(void *buf, i32 len)
{
    Buf *hdr = buf_hdr(buf);
    if (hdr) {
        hdr->len = len;
    } else {
        assert(len == 0);
    }
}

#define buf_remove_unsorted_index(buf, index) buf_remove_unsorted_(buf_hdr(buf), buf_elem_size(buf), index)
#define buf_remove_unsorted(buf, elem) buf_remove_unsorted_(buf_hdr(buf), buf_elem_size(buf), buf_index_of(buf, elem))
force_inline
void buf_remove_unsorted_(Buf *hdr, isize size, isize index)
{
    if (ALWAYS(hdr && index >= 0 && index < hdr->len)) {
        memmove(hdr_buf(hdr) + index * size, hdr_buf(hdr) + (hdr->len - 1) * size, size);
        hdr->len--;
    }
}

#define buf_remove_sorted_index(buf, index) buf_remove_sorted_(buf_hdr(buf), buf_elem_size(buf), index)
#define buf_remove_sorted(buf, elem) buf_remove_sorted_(buf_hdr(buf), buf_elem_size(buf), buf_index_of(buf, elem))
force_inline
void buf_remove_sorted_(Buf *hdr, isize size, isize index)
{
    if (ALWAYS(hdr && index >= 0 && index < hdr->len)) {
        memmove(hdr_buf(hdr) + index * size, hdr_buf(hdr) + (index + 1) * size, (hdr->len - index - 1) * size);
        hdr->len--;
    }
}

typedef struct String
{
    char *buf;
    isize len;
} String;
#define S(imm) ((String) { .buf = imm, .len = sizeof(imm) - 1 })
// msvc in its infinite wisdom needs this for static init
#define SS(imm) { .buf = imm, .len = sizeof(imm) - 1 }

b32 strings_are_equal(String a, String b)
{
    if (a.len == b.len) {
        return memcmp(a.buf, b.buf, a.len) == 0;
    }

    return false;
}

String copy_string(String s)
{
    String result = {0};
    result.len = buf_printf(result.buf, "%.*s", s.len, s.buf);
    buf_push(result.buf, 0);
    return result;
}

static Stack **stack_stack = 0;
i32 push_allocator(Stack *a)
{
    Stack **pushed = buf_push(stack_stack, current_allocator);
    current_allocator = a;
    return (i32)buf_index_of(stack_stack, pushed);
}

void pop_allocator()
{
    current_allocator = buf_pop(stack_stack);
}

void restore_allocator(i32 handle)
{
    assert(handle >= 0);
    assert(handle < buf_len(stack_stack));
    buf_set_len(stack_stack, handle);
    current_allocator = handle > 0 ? buf_last(stack_stack) : permanent_memory;
}
