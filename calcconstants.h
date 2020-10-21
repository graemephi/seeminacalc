// Inline floating point values in the calc source have been marked up like
// P(0.0f). If DUMP_CONSTANTS is 1, then P expands to store the location and
// value of the float. The first time you bring up the debug menu (`), these
// stored values get dumped as a static array (`minacalc_constant`) in
// calcconstants.gen.h, along with some other information so the GUI can make
// sense of it.
//
// If DUMP_CONSTANTS is 0, then calcconstants.gen.h is included, and P expands
// to `minacalc_constant[__COUNTER__]`. So, inline constants can be changed at
// runtime, and the perf hit is there, but not too bad.
#define DUMP_CONSTANTS 0

typedef enum {
    Constant_Skip,
    Constant_Real,
    Constant_Copy,
} ConstantType;

typedef struct InlineConstantInfo {
    char const *file;
    int line;
    int nth;
    int index;
    bool optimizable;
} InlineConstantInfo;

#ifndef __cplusplus
extern struct {
    unsigned char *file;
    int file_id;
    int line;
    float value;
    ConstantType type;
} constant_info[1024];

b32 str_eq(char const *a, char const *b)
{
    return strcmp(a, b) == 0;
}

static u8 *filename(u8 *file)
{
    isize end = strlen(file);
    isize i = end - 1;
    while (i-- > 0) {
        if (file[i] == '/' || file[i] == '\\') {
            break;
        }
    }
    isize slash = i + 1;
    u8 *result = calloc(end - slash + 1, sizeof(u8));
    memcpy(result, file + slash, end - slash);
    assert(result[end - slash] == 0);
    return result;
}

static u8 *relative_path(u8 *file)
{
    isize size = strlen(file);
    u8 *result = calloc(size + 1, sizeof(u8));
    memcpy(result, file, size);
    for (isize i = 0; i < size; i++) {
        if (result[i] == '\\') {
            result[i] = '/';
        }
    }

    u8 *root = strstr(result, "etterna/Etterna");
    isize len = strlen(root);
    memmove(result, root, len);
    result[len] = 0;
    return result;
}

static void dump_constant_info_to_file(void)
{
    static b32 once = false;
    if (once) {
        return;
    }
    once = true;

    u8 nth_of_line[array_length(constant_info)] = {0};
    i32 prev_fixup[array_length(constant_info)] = {0};
    i32 indices[array_length(constant_info)] = {0};

    u8 *gen = 0;
    isize last_constant = 0;
    for (isize i = array_length(constant_info) - 1; i >= 0; i--) {
        if (constant_info[i].file != 0) {
            last_constant = i + 1;
            break;
        }
    }

    buf_printf(gen, "// File generated by calcconstants.h\n\n");
    buf_printf(gen, "static float null_constant = 0.0f;\n");
    buf_printf(gen, "thread_local float minacalc_constant[] = {");
    for (isize i = 0; i < last_constant; i++) {
        if ((i & 7) == 0) {
            buf_printf(gen, "\n    ");
        }
        buf_printf(gen, " %a,", constant_info[i].value);
    }
    buf_printf(gen, "\n};\n\n");

    buf_printf(gen, "thread_local std::vector<std::pair<std::string, float*>> MinaCalcConstants {\n");
    i32 index = 0;
    for (isize i = 0; i < last_constant; i++) {
        if (constant_info[i].type == Constant_Skip) {
            continue;
        }
        if (constant_info[i].file) {
            // Mod prefix, for display as Mod.param
            u8 *mod = filename(constant_info[i].file);

            // Figure out how many floats are on this line of code
            isize k = i;
            while (constant_info[i].file_id == constant_info[k].file_id && constant_info[i].line == constant_info[k].line) {
                k--;
            }
            k++;

            // nth float
            isize n = i - k + 1;
            nth_of_line[i] = (u8)n - 1;

            // Provisionally set and increment the index of this param in the
            // MinaCalcConstants array. This is implicit when written but we
            // need to track it to set index in the InlineConstanInfo metadata.
            indices[i] = index++;

            // Fix up PREV constants. We point the mod pointer at some previous
            // paramater, and store the index in prev_fixup so the constant in
            // the source can be redirected, too.
            isize constants_index = i;
            if (constant_info[i].type == Constant_Copy) {
                constants_index -= 1;
                while (constant_info[constants_index].value != constant_info[i].value
                    || constant_info[constants_index].type != Constant_Real) {
                    constants_index -= 1;
                }
                assert(constant_info[constants_index].value == constant_info[i].value);
                assert(constant_info[constants_index].type == Constant_Real);
                prev_fixup[i] = (i32)constants_index;
                indices[i] = indices[constants_index];
            }

            if (n == 1) {
                buf_printf(gen, "    { \"%s(%d)\", &minacalc_constant[%zd] },\n", mod, constant_info[i].line, constants_index);
            } else {
                buf_printf(gen, "    { \"%s(%d, %zd)\", &minacalc_constant[%zd] },\n", mod, constant_info[i].line, n, constants_index);
            }

            free(mod);
        }
    }
    buf_printf(gen, "};\n\n");

    buf_printf(gen, "static InlineConstantInfo where_u_at[] = {\n");
    for (isize i = 0; i < last_constant; i++) {
        if (constant_info[i].type == Constant_Skip) {
            continue;
        }
        if (constant_info[i].file) {
            u8 *path = relative_path(constant_info[i].file);
            buf_printf(gen, "    { \"%s\", %d, %d, %d, %d },\n", path, constant_info[i].line, nth_of_line[i], indices[i], constant_info[i].type == Constant_Real);
            free(path);
        }
    }
    buf_printf(gen, "};\n\n");

    buf_printf(gen, "static int minacalc_constant_prev_fixup[] = {");
    for (isize i = 0; i < last_constant; i++) {
        if ((i & 7) == 0) {
            buf_printf(gen, "\n    ");
        }
        buf_printf(gen, " %2d,", prev_fixup[i]);
    }
    buf_printf(gen, "\n};\n\n");

    write_file("calcconstants.gen.h", gen);
}

isize find_string(u8 *buf, String str)
{
    isize result = -1;
    isize i = 0;
    for (u8 *cursor = buf; *cursor; i++, cursor++) {
        if (string_equals_cstr(str, cursor)) {
            result = i;
            break;
        }
    }

    return result;
}

u8 *replace(char const *buf, String from, String to)
{
    u8 *result = 0;
    isize pos = find_string((u8 *)buf, from);
    if (ALWAYS(pos >= 0)) {
        buf_printf(result, "%.*s%s%s", pos, buf, to.buf, buf + pos + from.len);
    }
    return result;
}

u8 *get_calc_source_path_to_read(char const *file)
{
    static char **rewritten = 0;
    b32 use_rewrite = false;
    for (isize i = 0; i < buf_len(rewritten); i++) {
        if (str_eq(rewritten[i], file)) {
            use_rewrite = true;
        }
    }

    String new_root = S("etterna/_MinaCalc441");
    if (use_rewrite) {
        new_root = S("etterna/_MinaCalcRewrite");
    } else {
        push_allocator(permanent_memory);
        buf_push(rewritten, (char *)file);
        pop_allocator();
    }
    return replace(file, S("etterna/Etterna/MinaCalc"), new_root);
}

u8 *get_calc_source_path_to_write(char const *file)
{
    return replace(file, S("etterna/Etterna/MinaCalc"), S("etterna/_MinaCalcRewrite"));
}

Buffer read_calc_source(char const *file)
{
    u8 *path = get_calc_source_path_to_read(file);
    return read_file((char const *)path);
}

void write_calc_source(char const *file, char const *mod_name, u8 *rewrite)
{
    u8 *path = get_calc_source_path_to_write(file);
    write_file((char const *)path, rewrite);
    printf("Wrote %s to %s\n", mod_name, path);
}

char const *float_suffix(f32 value)
{
    if (value == 0.0f || floorf(value) == value) {
        return ".F";
    }

    return "F";
}

typedef struct {
    char const *file;
    Buffer source;
    isize offset;
    u8 *buffer;
    u8 *rewrite;
} FileRewriter;

FileRewriter file_rewriter_mod(char const *file, ModInfo *mod)
{
    Buffer source = read_calc_source(file);
    assert(source.len);
    // Some (well, one) files contain multiple mods
    isize mod_offset = find_string(source.buf, (String) { (u8 *)mod->name, strlen(mod->name )});
    assert(mod_offset >= 0);
    FileRewriter result = {
        .file = file,
        .source = source,
        .offset = mod_offset
    };
    buf_printf(result.rewrite, "%.*s", source.len, source.buf);
    return result;
}

FileRewriter file_rewriter(char const *file)
{
    Buffer source = read_calc_source(file);
    FileRewriter result = {
        .file = file,
        .source = source,
    };
    buf_printf(result.rewrite, "%.*s", source.len, source.buf);
    return result;
}

void rewrite_declaration(FileRewriter *fr, char const *name, f32 value)
{
    u8 *rewrite = fr->rewrite;

    isize dot = find_string((u8 *)name, S("."));
    if (dot >= 0) {
        name += dot + 1;
    }

    u8 *def = 0;
    buf_printf(def, "float %s =", name);
    isize start = find_string(rewrite + fr->offset, SB(def));
    assert(start >= 0);
    isize end = start + find_string(rewrite + fr->offset + start, S(";"));
    assert(end > start);

    start += fr->offset;
    end += fr->offset;

    buf_clear(fr->buffer);
    buf_printf(fr->buffer, "%.*sfloat %s = %g%s%s", start, rewrite, name, value, float_suffix(value), rewrite + end);

    fr->rewrite = fr->buffer;
    fr->buffer = rewrite;
}

typedef struct {
    isize pos;
    isize len;
} FloatPosition;

FloatPosition find_next_float_in_line(u8 *buf)
{
    u8 *cursor = buf;
    u8 *end = 0;
    while (*cursor != '\n') {
        while ((*cursor < '+' || *cursor > '9') && *cursor != '\n') {
            cursor++;
        }
        strtof(cursor, &end);
        if (end - cursor > 1) {
            break;
        }
        cursor++;
    }
    assert(*end == 'f' || *end == 'F');
    FloatPosition result = { cursor - buf, end + 1 - cursor };
    return result;
}

void rewrite_constant(FileRewriter *fr, isize line, isize floats_to_skip, f32 value)
{
    u8 *rewrite = fr->rewrite;
    isize start = 0;
    for (isize i = 1; i < line; i++) {
        while (rewrite[start++] != '\n') {
            ;
        }
    }
    FloatPosition p = {0};
    for (isize i = 0; i <= floats_to_skip; i++) {
        start += p.len;
        p = find_next_float_in_line(rewrite + start);
        assert(p.len);
        start += p.pos;
    }

    isize end = start + p.len;

    buf_clear(fr->buffer);
    buf_printf(fr->buffer, "%.*s%g%s%s", start, rewrite, value, float_suffix(value), rewrite + end);

    fr->rewrite = fr->buffer;
    fr->buffer = rewrite;
}

void rewrite_mod(CalcInfo *info, ModInfo *mod, ParamSet *ps)
{
    push_allocator(scratch);
    char const *file = file_for_param(info, mod->index);
    FileRewriter fr = file_rewriter_mod(file, mod);

    for (isize p = 0; p < mod->num_params; p++) {
        assert(str_eq(file, file_for_param(info, mod->index + p)));
        ParamInfo *param = &info->params[mod->index + p];
        f32 value = ps->params[mod->index + p];
        rewrite_declaration(&fr, param->name, value);
    }

    write_calc_source(file, mod->name, fr.rewrite);
    pop_allocator();
}

void rewrite_basescalers(CalcInfo *info, ModInfo *mod, ParamSet *ps)
{
    push_allocator(scratch);
    char const *file = file_for_param(info, mod->index);
    Buffer source = read_calc_source(file);

    u8 *basescalers = 0;
    buf_printf(basescalers, "0.F, ");
    isize last = mod->num_params - 1;
    for (isize i = 0; i < mod->num_params; i++) {
        buf_printf(basescalers, "%g%s%s", ps->params[mod->index + i], float_suffix(ps->params[mod->index + i]), i != last ? ", " : "");
    }

    isize start = find_string(source.buf, S("basescalers = {"));
    isize end = start + find_string(source.buf + start, S(";"));
    u8 *rewrite = 0;
    buf_printf(rewrite, "%.*sbasescalers = {\n\t%s\n}%s", start, source.buf, basescalers, source.buf + end);

    write_calc_source(file, mod->name, rewrite);
    pop_allocator();
}

void rewrite_globals(CalcInfo *info, ParamSet *ps, isize param_index, isize num_params)
{
    push_allocator(scratch);
    char const *file = file_for_param(info, param_index);
    FileRewriter fr = file_rewriter(file);

    for (isize p = 0; p < num_params; p++) {
        assert(str_eq(file, file_for_param(info, param_index + p)));
        ParamInfo *param = &info->params[param_index + p];
        f32 value = ps->params[param_index + p];
        rewrite_declaration(&fr, param->name, value);
    }

    write_calc_source(file, "Globals", fr.rewrite);
    pop_allocator();
}

void rewrite_constants(CalcInfo *info, ParamSet *ps, isize mod_index, isize start_param, isize num_params)
{
    push_allocator(scratch);
    InlineConstantInfo icf = *info_for_inline_constant(info, mod_index + start_param);
    char const *file = icf.file;
    FileRewriter fr = file_rewriter(file);

    for (isize p = 0; p < num_params; p++) {
        icf = *info_for_inline_constant(info, mod_index + start_param + p);
        assert(str_eq(file, icf.file));
        f32 value = ps->params[mod_index + icf.index];
        rewrite_constant(&fr, icf.line, icf.nth, value);
    }

    write_calc_source(file, "InlineConstants", fr.rewrite);
    pop_allocator();
}

void rewrite_parameters(CalcInfo *info, ParamSet *ps)
{

    static b32 once = false;
    if (once) {
        return;
    }
    once = true;

    for (isize m = 1; m < info->num_mods; m++) {
        ModInfo *mod = &info->mods[m];
        if (info->params[mod->index].constant == false) {
            rewrite_mod(info, mod, ps);
        } else if (string_equals_cstr(S("BaseScalers"), mod->name)) {
            rewrite_basescalers(info, mod, ps);
        } else if (string_equals_cstr(S("Globals"), mod->name)) {
            char const *file = file_for_param(info, mod->index);
            isize p = 0;
            while (p < mod->num_params) {
                isize start = p;
                for (; p < mod->num_params; p++) {
                    char const *next_file = file_for_param(info, mod->index + p);
                    if (str_eq(file, next_file) == false) {
                        file = next_file;
                        break;
                    }
                }
                isize end = p;
                rewrite_globals(info, ps, mod->index + start, end - start);
            }
        } else if (string_equals_cstr(S("InlineConstants"), mod->name)) {
            char const *file = info_for_inline_constant(info, mod->index)->file;
            isize p = 0;
            while (p < mod->num_params) {
                isize start = p;
                for (; p < mod->num_params; p++) {
                    char const *next_file = info_for_inline_constant(info, mod->index + p)->file;
                    if (str_eq(file, next_file) == false) {
                        file = next_file;
                        break;
                    }
                }
                isize end = p;
                rewrite_constants(info, ps, mod->index, start, end - start);
            }
        }
    }
}

#if DUMP_CONSTANTS
#define DUMP_CONSTANT_INFO dump_constant_info_to_file()
#else
#define DUMP_CONSTANT_INFO
#endif

#endif // not(__cplusplus)

#ifdef __cplusplus
extern "C" struct {
    unsigned char *file;
    int file_id;
    int line;
    float value;
    ConstantType type;
} constant_info[1024] = {0};

#if DUMP_CONSTANTS
static void add_constant_info(int id,  char const *file, int line, float value, ConstantType type)
{
    assert(id < sizeof(constant_info)/sizeof(constant_info[0]));
    bool is_minacalc_cpp = strstr(file, "MinaCalc.cpp") > 0;

    // Pretend stupud hack doesn't exist
    if (is_minacalc_cpp) {
        // these are line numbers
        if (line > 500) {
            line -= (528 - 500);
        }
        line -= (52 - 17);
    }

    if (constant_info[id].file == 0) {
        int file_id = 9377747;
        for (int i = 0, len = strlen(file); i < len; i++) {
            file_id ^= file[i];
            file_id *= 1595813;
        }
        constant_info[id] = {
            (unsigned char *)file,
            file_id,
            line,
            value,
            type,
        };
    }
}

#define N(value) (add_constant_info(__COUNTER__, __FILE__, __LINE__, value, Constant_Skip), (value))
#define P(value) (add_constant_info(__COUNTER__, __FILE__, __LINE__, value, Constant_Real), (value))
#define PREV(value) (add_constant_info(__COUNTER__, __FILE__, __LINE__, value, Constant_Copy), (value))
thread_local std::vector<std::pair<std::string, float*>> MinaCalcConstants;
static float null_constant = 0.0f;
InlineConstantInfo where_u_at[512];
#else // DUMP_CONSTANTS
#include "calcconstants.gen.h"
#define P(value) minacalc_constant[__COUNTER__]
#define PREV(value) minacalc_constant[minacalc_constant_prev_fixup[__COUNTER__]]
#define N(value) (__COUNTER__, (value))
#endif

#endif // __cplusplus
