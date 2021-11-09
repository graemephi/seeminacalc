// Buncha top-of-the-file crap that will just confuse anyone trying to look at
// seeminacalc.c to see where the good stuff is.

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define alignof _Alignof

#if defined(_MSC_VER) && !defined(alignas)
#define alignas(n) __declspec(align(n))
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#else
#define alignas(n) _Alignas(n)
#endif

#ifdef _MSC_VER

#if !defined(static_assert)
#define static_assert(cond) static_assert(cond, #cond)
#endif

#if defined(min)
#undef min
#undef max
#endif

#if defined(DEBUG)
// force_inline is a debugging aid mostly
// So force msvc to emit em
#define force_inline __forceinline extern
#else
#define force_inline
#endif

#define thread_local __declspec(thread)
#else
#define thread_local _Thread_local
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

static isize clamps(isize a, isize b, isize t)
{
    return maxs(a, mins(b, t));
}

static isize clamp_lows(isize a, isize b)
{
    return (a > b) ? a : b;
}

static isize clamp_highs(isize a, isize b)
{
    return (a <= b) ? a : b;
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

static f64 clamp_highd(f64 a, f64 b)
{
    return (a <= b) ? a : b;
}

static f32 clamp(f32 a, f32 b, f32 t)
{
    return clamp_low(a, clamp_high(b, t));
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

static f32 square(f32 a)
{
    return a*a;
}

static f32 absolute_value(f32 v)
{
    return (v < 0) ? -v : v;
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

isize stack_space_remaining(Stack *stack)
{
    return stack->end - stack->ptr;
}

void stack_reset(Stack *stack)
{
    stack->ptr = stack->buf;
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
        printf("oom :(\n");
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
thread_local Stack *current_allocator = 0;
Stack *scratch = &scratch_stack;
Stack *permanent_memory = &permanent_memory_stack;
#define alloc_scratch(type, count) stack_alloc(scratch, type, count)
#define alloc(type, count) stack_alloc(current_allocator, type, count)

void setup_allocators(void)
{
    if (scratch_stack.buf == 0) {
        assert(permanent_memory_stack.buf == 0);
        isize bignumber = 200*1024*1024;
        scratch_stack = stack_make(malloc(bignumber), bignumber);
        permanent_memory_stack = stack_make(malloc(bignumber), bignumber);
        current_allocator = permanent_memory;
    }
}

void reset_scratch(void)
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

typedef struct Buf
{
    Stack *alloc;
    isize len;
    isize cap;
    i32 leaked;
    i32 cookie;
} Buf;
static_assert(alignof(Buf) == sizeof(usize));
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
    if (count > 0) {
        buf_hdr(buf)->len += count;
        memset(result, 0, size * count);
        return result;
    }
    return result;
}

#define buf_zeros(buf, n) (buf_maybe_fit((buf), (n)), buf_zeros_(buf, buf_elem_size(buf), (n)))
force_inline
void buf_zeros_(void *buf, isize size, isize count)
{
    buf_clear(buf);
    buf_pushn_(buf, size, count);
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
        isize cap = 0;
        if (count > hdr->cap) {
            cap = hdr->cap + count;
        } else {
            cap = hdr->cap * 2;
        }

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

    assert(hdr->len < hdr->cap);
    hdr->cookie = BufCookie;
    return hdr + 1;
}

#define buf_printf(buf, ...) buf_printf_(&(buf), __VA_ARGS__)
i32 buf_printf_(u8 **buf_ref, char *fmt, ...)
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

#define buf_set_len(buf, len) (buf_reserve(buf, len), buf_set_len_(buf, len))
void buf_set_len_(void *buf, isize len)
{
    Buf *hdr = buf_hdr(buf);
    if (hdr) {
        if (len < 0) {
            len = hdr->len + len;
        }
        assert(len <= hdr->cap);
        hdr->len = mins(len, hdr->cap);
    } else {
        assert(len == 0);
    }
}

#define buf_remove_unsorted_index(buf, index) buf_remove_unsorted_(buf_hdr(buf), buf_elem_size(buf), index)
#define buf_remove_unsorted(buf, elem) buf_remove_unsorted_(buf_hdr(buf), buf_elem_size(buf), buf_index_of(buf, elem))
void buf_remove_unsorted_(Buf *hdr, isize size, isize index)
{
    if (ALWAYS(hdr && index >= 0 && index < hdr->len)) {
        memmove(hdr_buf(hdr) + index * size, hdr_buf(hdr) + (hdr->len - 1) * size, size);
        hdr->len--;
    }
}

#define buf_remove_sorted_index(buf, index) buf_remove_sorted_(buf_hdr(buf), buf_elem_size(buf), index)
#define buf_remove_sorted(buf, elem) buf_remove_sorted_(buf_hdr(buf), buf_elem_size(buf), buf_index_of(buf, elem))
void buf_remove_sorted_(Buf *hdr, isize size, isize index)
{
    if (ALWAYS(hdr && index >= 0 && index < hdr->len)) {
        memmove(hdr_buf(hdr) + index * size, hdr_buf(hdr) + (index + 1) * size, (hdr->len - index - 1) * size);
        hdr->len--;
    }
}

#define buf_remove_first_n(buf, n) buf_remove_first_n_(buf_hdr(buf), buf_elem_size(buf), n)
void buf_remove_first_n_(Buf *hdr, isize size, isize n)
{
    if (hdr && n > 0) {
        if (n >= hdr->len) {
            hdr->len = 0;
            return;
        }
        isize remaining = hdr->len - n;
        memmove(hdr_buf(hdr), hdr_buf(hdr) + n * size, size * remaining);
        hdr->len -= n;
    }
}

typedef struct String
{
    u8 *buf;
    isize len;
} String;
#define S(imm) ((String) { .buf = imm, .len = sizeof(imm) - 1 })
#define SB(b) ((String) { .buf = b, .len = buf_len(b) })
// msvc in its infinite wisdom needs this for static init
#define SS(imm) { .buf = imm, .len = sizeof(imm) - 1 }

b32 strings_are_equal(String a, String b)
{
    if (a.len == b.len) {
        return memcmp(a.buf, b.buf, a.len) == 0;
    }

    return false;
}

b32 string_equals_cstr(String a, const char *b)
{
    return strncmp(a.buf, b, a.len) == 0;
}

String copy_string(String s)
{
    String result = {0};
    result.len = buf_printf(result.buf, "%.*s", s.len, s.buf);
    buf_push(result.buf, 0);
    return result;
}

i32 string_to_i32(String s)
{
    return atoi(s.buf);
}

f32 string_to_f32(String s)
{
    return strtof(s.buf, 0);
}

Stack **stack_stack = 0;
i32 push_allocator(Stack *a)
{
    Stack **pushed = buf_push(stack_stack, current_allocator);
    current_allocator = a;
    return (i32)buf_index_of(stack_stack, pushed);
}

void pop_allocator(void)
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

u64 rng(void)
{
    // wikipedia, xorshift
    static u64 x = 1;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

// I USe this in the optizimation code so i figure it may as well be unbiased! alright! ok!
u64 rngu(u64 upper)
{
    if (upper == 0) {
        return 0;
    }
    u64 mask = upper | (upper >> 1);
    mask |= (mask >> 2);
    mask |= (mask >> 4);
    mask |= (mask >> 8);
    mask |= (mask >> 16);
    mask |= (mask >> 32);
    u64 result = 0;
    do {
        result = rng() & mask;
    } while (result >= upper);
    return result;
}

f32 rngf(void)
{
    u32 a = rng() & ((1 << 23) - 1);
    return (f32)a / (f32)(1 << 23);
}

Buffer read_file_really(const char *path)
{
    Buffer result = {0};
    FILE *f = fopen(path, "rb");
    if (!f) {
        goto bail;
    }

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);

    result = (Buffer) {
        alloc(u8, filesize + 1),
        filesize + 1,
        filesize + 1
    };

    if (!result.buf) {
        goto bail;
    }

    usize read = fread(result.buf, 1, filesize, f);

    if (read != filesize) {
        goto bail;
    }

    fclose(f);

    result.buf[read] = 0;
    return result;

bail:
    if (f) {
        fclose(f);
    }
    result = (Buffer) {0};
    return result;
}

void write_file_really(const char *path, u8 buf[])
{
    FILE *f = fopen(path, "wb");
    if (f) {
        int written = fwrite(buf, 1, buf_len(buf), f);
        assert(written == buf_len(buf));
        fclose(f);
    } else {
        printf("Couldn't open %s for writing\n", path);
    }
}

Buffer read_file_malloc_really(const char *path)
{
    Buffer result = {0};
    FILE *f = fopen(path, "rb");
    if (!f) {
        goto bail;
    }

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);

    result = (Buffer) {
        calloc(sizeof(u8), filesize + 1),
        filesize + 1,
        filesize + 1
    };

    if (!result.buf) {
        goto bail;
    }

    usize read = fread(result.buf, 1, filesize, f);

    if (read != filesize) {
        goto bail;
    }

    fclose(f);

    result.buf[read] = 0;
    return result;

bail:
    if (f) {
        fclose(f);
    }
    if (result.buf) {
        free(result.buf);
    }
    result = (Buffer) {0};
    return result;
}

#if !defined(__EMSCRIPTEN__)
Buffer read_file(const char *path) { return read_file_really(path); }
Buffer read_file_malloc(const char *path) { return read_file_malloc_really(path); }
void write_file(const char *path, u8 buf[]) { write_file_really(path, buf); }

#else // ^^^^ !defined(__EMSCRIPTEN__)
Buffer read_file(const char *path) {
    push_allocator(scratch);
    const char *p = 0;
    buf_printf(p, "/seeminacalc/%s", path);
    pop_allocator();
    return read_file_really(p);
}
Buffer read_file_malloc(const char *path) {
    push_allocator(scratch);
    const char *p = 0;
    buf_printf(p, "/seeminacalc/%s", path);
    pop_allocator();
    return read_file_malloc_really(p);
}
void write_file(const char *path, u8 buf[]) {
    push_allocator(scratch);
    const char *p = 0;
    buf_printf(p, "/seeminacalc/%s", path);
    pop_allocator();
    write_file_really(p, buf);
    EM_ASM(
        FS.syncfs((err) => {
            if (err) console.error(err);
        });
    );
}

EM_JS(void, emsc_file_dialog, (const char* name, const char *buf), {
    let garbage = document.createElement("a");
    garbage.href = window.URL.createObjectURL(new Blob([UTF8ToString(buf)], { type: "text/xml" }));
    garbage.download = UTF8ToString(name);
    garbage.click();
    garbage.remove();
});
#endif

b32 buffer_begins_with(Buffer *buffer, String s)
{
    return memcmp(buffer->buf, s.buf, mins(buffer->len, s.len)) == 0;
}

u8 buffer_first_nonwhitespace_char(Buffer *buffer)
{
    isize i = 0;
    if (buffer->len > 4) {
        // boms.. so many boms... whjy
        u32 first_word = *(u32 *)buffer->buf;
        if ((first_word & 0xffffff) == 0xBFBBEF) {
            i = 3;
        }
    }
    for (; i < buffer->len; i++) {
        if (isspace(buffer->buf[i]) == false) {
            return buffer->buf[i];
        }
    }
    return 0;
}
