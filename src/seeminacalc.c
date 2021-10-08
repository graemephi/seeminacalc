#if !(defined(SOKOL_GLCORE33)||defined(SOKOL_GLES2)||defined(SOKOL_GLES3)||defined(SOKOL_D3D11)||defined(SOKOL_METAL)||defined(SOKOL_WGPU)||defined(SOKOL_DUMMY_BACKEND))
#define SOKOL_GLCORE33
#endif

#ifdef __clang__
// for libs only
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#endif

#define SOKOL_NO_DEPRECATED
#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_args.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_glue.h"

#ifdef _MSC_VER
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#endif
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimgui/cimplot.h"
#define SOKOL_IMGUI_IMPL
#include "sokol/util/sokol_imgui.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "thread.h"
#include "bottom.h"
#include "cminacalc.h"
#include "sm.h"

#include "optimize.c"

#include "calcconstants.h"

#ifdef __EMSCRIPTEN__
static Buffer font_data = {0};

// Must be called before main. In theory we can just fread off Emscripten's VFS
// but it throws for me and I ain't looking into it, might as well just get the
// size reduction from not using it instead
void set_font(char *buf, int len)
{
    font_data = (Buffer) {
        .buf = buf,
        .len = len,
        .cap = len
    };
}

Buffer load_font_file(char *path)
{
    (void)path;
    Buffer result = (Buffer) {
        .buf = alloc(u8, font_data.len),
        .len = font_data.len,
        .cap = font_data.len
    };
    memcpy(result.buf, font_data.buf, font_data.len);
    free(font_data.buf);
    return result;
}

void open_file(char *data, int len, b32 open_window)
{
    if (permanent_memory->end - permanent_memory->ptr < 4*1024*1024) {
        printf("oom :(\n");
        return;
    }

    Buffer buf = (Buffer) {
        .buf = alloc(u8, len + 1),
        .len = len,
        .cap = len
    };

    memcpy(buf.buf, data, len);
    i32 parse_and_add_sm(Buffer buf, b32 open_window);
    parse_and_add_sm(buf, open_window);
    free(data);
}
#else
Buffer load_font_file(char *path)
{
    return read_file(path);
}
#endif

typedef struct {
    u8 *name;
    f32 *params;
    u32 generation;
} OptimizationCheckpoint;

typedef struct EffectMasks
{
    u8 *weak;
    u8 *strong;
} EffectMasks;

typedef struct SimFileInfo
{
    String title;
    String diff;
    String chartkey;
    String id;

    f32 aa_rating;
    f32 max_rating;

    i32 notes_len; // just for stats
    NoteData *notes;
    EffectMasks effects;
    i32 *graphs;

    struct {
        i32 index;
        f32 want_msd;
        f32 rate;
        i32 skillset;

        f32 msd_bias;
        f32 weight;

        f32 got_msd;
        f32 delta;
    } target;

    SkillsetRatings default_ratings;
    u32 num_effects_computed;
    u32 effects_generation;

    bool selected_skillsets[NumSkillsets];
    bool display_skillsets[NumSkillsets];

    bool open;
    bool stops;
    bool fullscreen_window;
    u64 frame_last_focused;
} SimFileInfo;

static SimFileInfo null_sfi_ = {0};
static SimFileInfo *const null_sfi = &null_sfi_;

typedef struct { f32 low; f32 high; } Bound;

typedef struct FnGraph FnGraph;
typedef struct CalcThread CalcThread;
typedef struct CalcWork CalcWork;
typedef struct State
{
    u64 last_time;
    sg_pass_action pass_action;

    CalcThread *threads;
    u32 generation;
    CalcWork *high_prio_work;
    CalcWork *low_prio_work;

    CalcInfo info;
    SeeCalc calc;
    SimFileInfo *files;
    SimFileInfo *active;

    ModInfo *inline_mods;

    ParamSet ps;
    bool *parameter_graphs_enabled;
    i32 *parameter_graph_order;
    FnGraph *graphs;
    i32 *free_graphs;

    OptimizationContext opt;
    OptimizationEvaluation *opt_evaluations;
    i32 opt_pending_evals;
    b32 optimizing;
    FnGraph *optimization_graph;
    struct {
        b8 *enabled;
        b8 *all_mod_enabled;
        f32 barriers[NumSkillsets];
        f32 goals[NumSkillsets];
        f32 *normalization_factors;
        Bound *bounds;
        struct {
            f32 add;
            f32 mul;
        } bias[NumSkillsets];
        struct {
            i32 *to_opt;
            i32 *to_ps;
        } map;
    } opt_cfg;

    OptimizationCheckpoint *checkpoints;
    OptimizationCheckpoint latest_checkpoint;
    OptimizationCheckpoint default_checkpoint;

    struct {
        f32 msd_mean;
        f32 msd_sd;

        SkillsetRatings average_delta;
        f32 min_delta;
        f32 max_delta;
    } target;

    u32 skillset_colors[NumSkillsets];
    u32 skillset_colors_selectable[NumSkillsets];

    int update_index;
    struct {
        i32 num_open_windows;
        f32 left_width;
        f32 centre_width;
        f32 right_width;
    } last_frame;

    b8 debug_window;
    b8 loss_window;
} State;
static State state = {0};

#define SEEMINACALC
#include "cachedb.c"
char const *db_path = 0;
char const *test_list_path = 0;

#include "graphs.c"

#pragma float_control(precise, on, push)
#include "sm.c"
#pragma float_control(pop)


OptimizationCheckpoint make_checkpoint(u8 *name)
{
    OptimizationCheckpoint result = {0};

    buf_printf(result.name, "%s", name);
    for (isize i = 0; i < state.info.num_params; i++) {
        buf_push(result.params, state.ps.params[i]);
    }

    return result;
}

void save_checkpoints_to_disk(void)
{
    u8 *str = 0;
    for (isize i = 0; i < buf_len(state.checkpoints); i++) {
        buf_printf(str, "#name: %s;\n#params: ", state.checkpoints[i].name);
        for (usize p = 0; p < state.ps.num_params; p++) {
            buf_printf(str, "%.8g,", (f64)state.checkpoints[i].params[p]);
        }
        buf_printf(str, ";\n\n");
    }

    write_file("checkpoints.txt", str);
}

// Reusing sm parser code because why not
static SmTagValue parse_checkpoint_tag(SmParser *ctx)
{
    static String tag_strings[] = {
        SS("name"),
        SS("params"),
    };
    consume_char(ctx, '#');
    i32 tag = array_length(tag_strings);
    for (i32 i = 0; i < array_length(tag_strings); i++) {
        if (try_consume_string(ctx, tag_strings[i])) {
            tag = i;
            break;
        }
    }
    if (tag == array_length(tag_strings)) {
        die(ctx, "tag");
    }

    consume_char(ctx, ':');
    isize start = parser_position(ctx);
    advance_to(ctx, ';');
    isize end = parser_position(ctx);
    consume_char(ctx, ';');
    return (SmTagValue) {
        .tag = tag,
        .str.index = (i32)start,
        .str.len = (i32)(end - start)
    };
}

static b32 try_advance_to_and_parse_checkpoint_tag(SmParser *ctx, SmTagValue *out_tag)
{
    consume_whitespace_and_comments(ctx);
    if (*ctx->p != '#') {
        return false;
    }
    *out_tag = parse_checkpoint_tag(ctx);
    return true;
}

void load_checkpoints_from_disk(void)
{
    Buffer f = read_file("checkpoints.txt");
    if (f.len) {
        jmp_buf env;
        SmParser *ctx = &(SmParser) {
            .buf = f.buf,
            .p = f.buf,
            .end = f.buf + f.len,
            .env = &env
        };
        i32 err = setjmp(env);
        if (err) {
            printf("checkpoints parse error: %s\n", ctx->error.buf);
            return;
        }

        for (SmTagValue s, t; try_advance_to_and_parse_checkpoint_tag(ctx, &s) && try_advance_to_and_parse_checkpoint_tag(ctx, &t);) {
            OptimizationCheckpoint cp = {0};
            buf_printf(cp.name, "%.*s", s.str.len, f.buf + s.str.index);
            SmParser param_ctx = *ctx;
            param_ctx.p = f.buf + t.str.index;
            for (usize i = 0; i < state.ps.num_params; i++) {
                buf_push(cp.params, parse_f32(&param_ctx));
                consume_char(&param_ctx, ',');
            }
            buf_push(state.checkpoints, cp);
        }
    }
}

void checkpoint(void)
{
    if (buf_len(state.checkpoints) > 0 && buf_last(state.checkpoints).generation == state.generation) {
        return;
    }
    OptimizationCheckpoint cp = { .generation = state.generation };
    time_t t = time(0);
    u8 *date = (u8 *)asctime(localtime(&t));
    date[strlen(date)-1] = 0; // null out new line
    buf_printf(cp.name, "%s", date);
    for (isize i = 0; i < state.info.num_params; i++) {
        buf_push(cp.params, state.ps.params[i]);
    }
    buf_push(state.checkpoints, cp);

    save_checkpoints_to_disk();
}

void checkpoint_default(void)
{
    buf_clear(state.default_checkpoint.name);
    buf_clear(state.default_checkpoint.params);

    buf_printf(state.default_checkpoint.name, "%s", "Default");
    for (isize i = 0; i < state.info.num_params; i++) {
        buf_push(state.default_checkpoint.params, state.ps.params[i]);
    }
    state.default_checkpoint.generation = state.generation;
}

void checkpoint_latest(void)
{
    buf_clear(state.latest_checkpoint.name);
    buf_clear(state.latest_checkpoint.params);

    buf_printf(state.latest_checkpoint.name, "%s", "Latest");
    for (isize i = 0; i < state.info.num_params; i++) {
        buf_push(state.latest_checkpoint.params, state.ps.params[i]);
    }
    state.latest_checkpoint.generation = state.generation;
}

void restore_checkpoint(OptimizationCheckpoint cp)
{
    for (isize i = 0; i < state.info.num_params; i++) {
        state.ps.params[i] = cp.params[i];
        state.generation++;
    }

    state.optimizing = false;
}

static const ImVec2 V2Zero = {0};

bool BeginPlotCppCpp(const char* title, const char* x_label, const char* y_label, const ImVec2* size, ImPlotFlags flags, ImPlotAxisFlags x_flags, ImPlotAxisFlags y_flags, ImPlotAxisFlags y2_flags, ImPlotAxisFlags y3_flags);
static bool BeginPlotDefaults(const char* title_id, const char* x_label, const char* y_label)
{
    return BeginPlotCppCpp(title_id, x_label, y_label, &(ImVec2){igGetWindowWidth() / 2.0f - 8.0f, 0}, ImPlotFlags_Default & ~ImPlotFlags_Legend, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary);
}
static bool BeginPlotDefaultsOptimizer(const char* title_id, const char* x_label, const char* y_label)
{
    return BeginPlotCppCpp(title_id, x_label, y_label, &(ImVec2){igGetWindowWidth() - 8.0f, 0}, ImPlotFlags_Default & ~ImPlotFlags_Legend, ImPlotAxisFlags_Auxiliary & ~ImPlotAxisFlags_TickLabels, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary);
}

static ImVec4 GetColormapColor(int index)
{
    ImVec4 result;
    ipGetColormapColor(&result, index);
    return result;
}

static bool ItemDoubleClicked(int button)
{
    bool result = igIsItemHovered(0) && igIsMouseDoubleClicked(button);
    if (result) {
        igClearActiveID();
    }
    return result;
}

static f32 GetContentRegionAvailWidth(void)
{
    ImVec2 cr;
    igGetContentRegionAvail(&cr);
    return cr.x;
}

static void tooltip(const char *fmt, ...)
{
    if (igIsItemHovered(0)) {
        igBeginTooltip();
        igPushTextWrapPos(400.0f);
        va_list args;
        va_start(args, fmt);
        igTextV(fmt, args);
        va_end(args);
        igPopTextWrapPos();
        igEndTooltip();
    }
}

static ImVec2 V2(f32 x, f32 y)
{
    return (ImVec2) { x, y };
}

static ImVec4 msd_color(f32 x)
{
    // -- Colorized stuff
    // function byMSD(x)
    //     if x then
    //         return HSV(math.max(95 - (x / 40) * 150, -50), 0.9, 0.9)
    //     end
    //     return HSV(0, 0.9, 0.9)
    // end
    ImVec4 color = { 0, 0.9f, 0.9f, 1.0f };
    if (x) {
        f32 h = max(95.0f - (x / 40.0f) * 150.0f, -50.0f) / 360.f;
        h = h + (f32)(h < 0) - truncf(h);
        f32 s = 0.9f;
        f32 v = 0.9f;
        igColorConvertHSVtoRGB(h, s, v, &color.x, &color.y, &color.z);
    }

    return color;
}

ParamSet copy_param_set(ParamSet *ps)
{
    ParamSet result = {0};
    result.num_params = ps->num_params;
    buf_reserve(result.params, ps->num_params);
    buf_reserve(result.min, ps->num_params);
    buf_reserve(result.max, ps->num_params);
    for (size_t i = 0; i < ps->num_params; i++) {
        result.params[i] = ps->params[i];
        result.min[i] = ps->min[i];
        result.max[i] = ps->max[i];
    }
    return result;
}

i32 make_skillsets_graph(void)
{
    FnGraph *fng = 0;
    if (buf_len(state.free_graphs) > 0) {
        fng = &state.graphs[buf_pop(state.free_graphs)];
    } else {
        fng = buf_pushn(state.graphs, 1);
    }
    *fng = (FnGraph) {0};
    fng->active = true;
    fng->is_param = false;
    fng->param = -1;
    fng->len = NumGraphSamples;
    fng->min = FLT_MAX;
    fng->max = FLT_MIN;
    fng->relative_min = FLT_MAX;
    fng->generation = 0;
    for (i32 x = 0; x < fng->len; x++) {
        fng->xs[x] = lerp(WifeXs[0] * 100.f, WifeXs[Wife965Index + 1] * 100.f, (f32)x / (f32)(fng->len - 1));
    }
    return (i32)buf_index_of(state.graphs, fng);
}

i32 make_parameter_graph(i32 param)
{
    FnGraph *fng = 0;
    if (buf_len(state.free_graphs) > 0) {
        fng = &state.graphs[buf_pop(state.free_graphs)];
    } else {
        fng = buf_pushn(state.graphs, 1);
    }
    *fng = (FnGraph) {0};
    fng->active = true;
    fng->is_param = true;
    fng->param = param;
    fng->len = state.info.params[param].integer ? (i32)state.info.params[param].max + 1 : NumGraphSamples;
    fng->min = FLT_MAX;
    fng->max = FLT_MIN;
    fng->relative_min = FLT_MAX;
    fng->generation = 0;
    if (state.info.params[param].integer) {
        for (i32 x = 0; x < fng->len; x++) {
            fng->xs[x] = (f32)x;
        }
    } else {
        for (i32 x = 0; x < fng->len; x++) {
            fng->xs[x] = lerp(state.ps.min[param], state.ps.max[param], (f32)x / (f32)(fng->len - 1));
        }
    }
    return (i32)buf_index_of(state.graphs, fng);
}

i32 make_optimization_graph(void)
{
    FnGraph *fng = 0;
    if (buf_len(state.free_graphs) > 0) {
        fng = &state.graphs[buf_pop(state.free_graphs)];
    } else {
        fng = buf_pushn(state.graphs, 1);
    }
    *fng = (FnGraph) {0};
    fng->active = true;
    fng->is_param = false;
    fng->param = -1;
    fng->len = NumGraphSamples;
    fng->min = 0.0f;
    fng->max = 100.0f;
    for (i32 x = 0; x < fng->len; x++) {
        fng->xs[x] = (f32)x;
    }
    return (i32)buf_index_of(state.graphs, fng);
}

void free_graph(i32 handle)
{
    state.graphs[handle].active = false;
    buf_push(state.free_graphs, handle);
}

void free_all_graphs(i32 handles[])
{
    for (i32 i = 0; i != buf_len(handles); i++) {
        i32 handle = handles[i];
        state.graphs[handle].active = false;
        buf_push(state.free_graphs, handle);
    }
    buf_clear(handles);
}

i32 parse_and_add_sm(Buffer buf, b32 open_window)
{
    if (buf_len(state.files) == buf_cap(state.files)) {
        printf("please.. no more files");
        return -1;
    }

    SmFile sm = {0};
    push_allocator(scratch);
    i32 err = parse_sm(buf, &sm);
    pop_allocator();
    if (err) {
        return -1;
    }

    String title = sm_tag_inplace(&sm, Tag_Title);
    String author = sm_tag_inplace(&sm, Tag_Credit);

    for (isize i = 0; i < buf_len(sm.diffs); i++) {
        NoteInfo *ni = sm_to_ett_note_info(&sm, (i32)i);
        push_allocator(scratch);
        String ck = generate_chart_key(&sm, i);
        String diff = SmDifficultyStrings[sm.diffs[i].diff];
        String id = {0};
        id.len = buf_printf(id.buf, "%.*s (%.*s%.*s%.*s)##%.*s",
            title.len, title.buf,
            author.len, author.buf,
            author.len == 0 ? 0 : 2, ", ",
            diff.len, diff.buf,
            ck.len, ck.buf
        );
        pop_allocator();

        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (strings_are_equal(sfi->id, id)) {
                return -2;
            }
        }

        SimFileInfo *sfi = buf_push(state.files, (SimFileInfo) {
            .title = copy_string(title),
            .diff = diff,
            .chartkey = copy_string(ck),
            .id = copy_string(id),
            .notes_len = (i32)buf_len(ni),
            .notes = frobble_note_data(ni, buf_len(ni)),
            .stops = sm.has_stops,
            .open = open_window,
            .selected_skillsets[0] = true,
        });

        buf_push(sfi->graphs, make_skillsets_graph());

        buf_reserve(sfi->effects.weak, state.info.num_params);
        buf_reserve(sfi->effects.strong, state.info.num_params);

        calculate_skillsets(&state.high_prio_work, sfi, true, state.generation);
        calculate_file_graph_force(&state.low_prio_work, sfi, state.generation);

        printf("Added %s\n", id.buf);
    }

    submit_work(&high_prio_work_queue, state.high_prio_work, state.generation);

    return 0;
}

// copy n paste
i32 add_target_files(void)
{
    TargetFile *target_files = load_test_files(db_path, test_list_path);

    f32 target_m = 0.0f;
    f32 target_s = 0.0f;

    for (isize i = 0; i < buf_len(target_files); i++) {
        push_allocator(scratch);
        TargetFile *target = &target_files[i];
        String ck = target->key;
        String title = target->title;
        String author = target->author;
        String diff = SmDifficultyStrings[target->difficulty];
        String id = {0};
        id.len = buf_printf(id.buf, "%.*s (%.*s%.*s%.*s)##%.*s%g.%d",
            title.len, title.buf,
            author.len, author.buf,
            author.len == 0 ? 0 : 2, ", ",
            diff.len, diff.buf,
            ck.len, ck.buf,
            (f64)target->rate,
            target->skillset
        );
        pop_allocator();

        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (strings_are_equal(sfi->id, id)) {
                goto skip;
            }
        }

        SimFileInfo *sfi = buf_push(state.files, (SimFileInfo) {
            .title = copy_string(title),
            .diff = diff,
            .chartkey = copy_string(ck),
            .id = copy_string(id),
            .target.index = (i32)i,
            .target.want_msd = target->target,
            .target.rate = target->rate,
            .target.skillset = target->skillset,
            .target.weight = target->weight,
            .notes_len = (i32)target->note_data.len,
            .notes = frobble_serialized_note_data(target->note_data.buf, target->note_data.len),
            .selected_skillsets[0] = true,
        });

        free(target->note_data.buf);
        buf_push(sfi->graphs, make_skillsets_graph());

        buf_reserve(sfi->effects.weak, state.info.num_params);
        buf_reserve(sfi->effects.strong, state.info.num_params);

        calculate_skillsets(&state.high_prio_work, sfi, true, state.generation);

        f32 old_m = target_m;
        f32 old_s = target_s;
        target_m = old_m + (target->target - old_m) / (f32)(i + 1);
        target_s = old_s + (target->target - old_m)*(target->target - target_m);

        printf("Added %s %s %.2fx: %.2f\n", SkillsetNames[target->skillset], title.buf, (f64)target->rate, (f64)target->target);
        skip:;
    }

    f32 target_var = target_s / (f32)(buf_len(target_files) - 1);
    state.target.msd_mean = target_m;
    state.target.msd_sd = sqrtf(target_var);

    submit_work(&high_prio_work_queue, state.high_prio_work, state.generation);

    state.loss_window = true;

    return 0;
}

enum {
    ParamSlider_Nothing,
    ParamSlider_OptimizingToggled,
    ParamSlider_GraphToggled,
    ParamSlider_ValueChanged,
    ParamSlider_LowerBoundChanged,
    ParamSlider_UpperBoundChanged
};

typedef struct ParamSliderChange
{
    i32 type;
    i32 param;
    f32 value;
} ParamSliderChange;

static void param_slider_widget(i32 param_idx, ParamSliderChange *out)
{
    if (state.info.params[param_idx].fake) {
        return;
    }
    assert(out);
    i32 type = ParamSlider_Nothing;
    f32 value = 0.0f;
    i32 mp = param_idx;
    igPushIDInt(mp);
    if (igCheckbox("##opt", &state.opt_cfg.enabled[mp])) {
        if (state.info.params[mp].optimizable) {
            type = ParamSlider_OptimizingToggled;
        } else {
            state.opt_cfg.enabled[mp] = false;
        }
    } tooltip("optimize this parameter");
    igSameLine(0, 4);
    if (igCheckbox("##graph", &state.parameter_graphs_enabled[mp])) {
        type = ParamSlider_GraphToggled;
    } tooltip("graph this parameter");
    igSameLine(0, 4);
    char slider_id[32];
    snprintf(slider_id, sizeof(slider_id), "##slider%d", mp);
    if (state.info.params[mp].integer) {
        i32 value_int = (i32)state.ps.params[mp];
        i32 low = (i32)state.ps.min[mp];
        i32 high = (i32)state.ps.max[mp];
        if (igSliderInt(slider_id, &value_int, low, high, "%d")) {
            state.ps.params[mp] = (f32)value_int;
            type = ParamSlider_ValueChanged;
            value = (f32)value_int;
        }
    } else {
        igSetNextItemWidth(igGetFontSize() * 16.5f);
        if (igSliderFloat(slider_id, &state.ps.params[mp], state.ps.min[mp], state.ps.max[mp], "%f", 1.0f)) {
            type = ParamSlider_ValueChanged;
            value = state.ps.params[mp];
        }
        tooltip(state.info.params[mp].name);
        if (ItemDoubleClicked(0)) {
            state.ps.params[mp] = state.info.defaults.params[mp];
            type = ParamSlider_ValueChanged;
            value = state.ps.params[mp];
        }

#if 0
        igSameLine(0, 4);
        f32 speed = (state.ps.max[mp] - state.ps.min[mp]) / 100.f;
        // min/max bounds
        b32 changed = false;
        b32 reset = false;
        igSetNextItemWidth(igGetFontSize() * 4.0f);
        if (igDragFloatRange2("##range",  &state.ps.min[mp], &state.ps.max[mp], speed, -100.0f, 100.0f, "%.1f", "%.1f", 1.0f)) {
            changed = true;
        }
        tooltip("min/max override (the defaults are guesses)");
        if (ItemDoubleClicked(0)) {
            reset = true;
        }
        if (changed || reset) {
            ImVec2 l, u;
            igGetItemRectMin(&l);
            igGetItemRectMax(&u);
            u.x = l.x + (u.x - l.x) * 0.5f;
            b32 left = igIsMouseHoveringRect(l, u, true);
            if (left) {
                state.ps.min[mp] = reset ? state.info.defaults.min[mp] : state.ps.min[mp];
                type = ParamSlider_LowerBoundChanged;
                value = state.ps.min[mp];
            } else {
                state.ps.max[mp] = reset ? state.info.defaults.max[mp] : state.ps.max[mp];
                type = ParamSlider_UpperBoundChanged;
                value = state.ps.max[mp];
            }
        }
#endif
    }
    igPopID();
    if (out && type != ParamSlider_Nothing) {
        assert(out->type == ParamSlider_Nothing);
        out->type = type;
        out->param = mp;
        out->value = value;
    }
}

void mod_param_sliders(ModInfo *mod, i32 mod_index, u8 *effects, u32 effect_mask, ParamSliderChange *changed_param)
{
    if (igTreeNodeExStr(mod->name, ImGuiTreeNodeFlags_DefaultOpen)) {
        i32 count = 0;
        for (i32 i = 0; i < mod->num_params; i++) {
            i32 mp = mod->index + i;
            if (effects == 0 || (effects[mp] & effect_mask) != 0) {
                count++;
            }
        }
        if (count) {
            if (count > 1) {
                if (igCheckbox("##opt_all", &state.opt_cfg.all_mod_enabled[mod_index])) {
                    for (i32 i = 0; i < mod->num_params; i++) {
                        i32 mp = mod->index + i;
                        if (effects == 0 || (effects[mp] & effect_mask) != 0) {
                            state.opt_cfg.enabled[mp] = state.info.params[mp].optimizable && state.opt_cfg.all_mod_enabled[mod_index];
                        }
                    }
                    changed_param->type = ParamSlider_OptimizingToggled;
                } tooltip("optimize visible");
            }
            for (i32 i = 0; i < mod->num_params; i++) {
                i32 mp = mod->index + i;
                if (effects == 0 || (effects[mp] & effect_mask) != 0) {
                    param_slider_widget(mp, changed_param);
                }
            }
        }
        igTreePop();
    }
}


char const *after_last_slash(char const *p)
{
    // spot the ub
    char const *cursor = p + strlen(p);
    while (cursor >= p && *cursor != '/') {
        cursor--;
    }
    return cursor + 1;
}

void inlines_param_sliders(i32 inlines_mod_index, u8 *effects, u32 effect_mask, ParamSliderChange *changed_param)
{
    for (i32 i = 0; i < buf_len(state.inline_mods); i++) {
        mod_param_sliders(&state.inline_mods[i], inlines_mod_index + i, effects, effect_mask, changed_param);
    }
}

static void skillset_line_plot(i32 ss, b32 highlight, FnGraph *fng, f32 *ys)
{
    // Recreate highlighting cause ImPlot doesn't let you tell it to highlight lines
    if (highlight) {
        ipPushStyleVarFloat(ImPlotStyleVar_LineWeight, ipGetStyle()->LineWeight * 2.0f);
    }

    ipPushStyleColorU32(ImPlotCol_Line, state.skillset_colors[ss]);
    ipPlotLineFloatPtrFloatPtr(SkillsetNames[ss], fng->xs, ys, fng->len, 0, sizeof(float));
    ipPopStyleColor(1);

    if (highlight) {
        ipPopStyleVar(1);
    }
}

isize param_index_by_name(CalcInfo *info, String mod, String param)
{
    isize idx = -1;
    isize first_param_of_mod = 0;
    isize m = 0;
    for (; m < info->num_mods; m++) {
        if (string_equals_cstr(mod, info->mods[m].name)) {
            first_param_of_mod = info->mods[m].index;
            break;
        }
    }
    for (isize p = first_param_of_mod; p < info->num_params; p++) {
        if (string_equals_cstr(param, info->params[p].name)) {
            assert(info->params[p].mod == m);
            idx = p;
            break;
        }
    }
    assert(idx > 0);
    return idx;
}

i32 pack_opt_parameters(CalcInfo *info, f32 *params, f32 *normalization_factors, b8 *enabled, f32 *x, i32 *to_ps, i32 *to_opt)
{
    i32 n_params = (i32)info->num_params;
    i32 packed_i = 0;
    to_opt[0] = Param_None;
    for (i32 i = 1; i < n_params; i++) {
        if (enabled[i]) {
            to_ps[packed_i] = i;
            to_opt[i] = packed_i;
            x[packed_i] = params[i] / normalization_factors[i];
            packed_i++;
        }
    }
    return packed_i;
}

static f32 SkillsetOverallBalance = 0.35f;
static f32 UnLogScale = 0.5f;
static f32 Scale = 1.0f;
static f32 Misclass = 1.0f;
void setup_optimizer(void)
{
    f32 *normalization_factors = 0;
    f32 *initial_x = 0;
    push_allocator(scratch);
    buf_pushn(initial_x, state.ps.num_params);
    pop_allocator();

    NegativeEpsilon = 0.5f / state.target.msd_sd;

    buf_pushn(normalization_factors, state.ps.num_params);
    buf_pushn(state.opt_cfg.enabled, state.ps.num_params);
    buf_pushn(state.opt_cfg.all_mod_enabled, state.ps.num_params);
    buf_pushn(state.opt_cfg.map.to_ps, state.ps.num_params + 1);
    // This sets to_ps[-1] = to_ps[Param_None] = 0
    state.opt_cfg.map.to_ps = state.opt_cfg.map.to_ps + 1;
    buf_pushn(state.opt_cfg.map.to_opt, state.ps.num_params);
    // The param at 0, rate, is not a real parameter
    for (i32 i = 1; i < state.ps.num_params; i++) {
        // source inline constants can be optimized, but opt-in only
        state.opt_cfg.enabled[i] = false &&  state.info.params[i].optimizable && !state.info.params[i].constant;
        // since we use divided difference to get the derivative we scale all parameters before feeding them to the optimizer
        normalization_factors[i] = clamp_low(1.0f, fabsf(state.info.params[i].default_value));
    }
    i32 n_params = pack_opt_parameters(&state.info, state.ps.params, normalization_factors, state.opt_cfg.enabled, initial_x, state.opt_cfg.map.to_ps, state.opt_cfg.map.to_opt);
    i32 n_samples = (i32)buf_len(state.files);
    state.opt = optimize(n_params, initial_x, n_samples);
    state.opt_cfg.normalization_factors = normalization_factors;
    // some parameters can blow up the calc
    buf_pushn(state.opt_cfg.bounds, state.ps.num_params);
    for (i32 i = 0; i < state.ps.num_params; i++) {
        state.opt_cfg.bounds[i].low = -INFINITY;
        state.opt_cfg.bounds[i].high = INFINITY;
    }
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("StreamMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("StreamMod"), S("jack_comp_min"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("StreamMod"), S("jack_comp_max"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("JSMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("HSMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("CJMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("CJMod"), S("total_prop_scaler"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("RunningManMod"), S("offhand_tap_prop_min"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("WideRangeRollMod"), S("max_mod"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("Globals"), S("MinaCalc.tech_pbm"))].low = 1.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("Globals"), S("MinaCalc.jack_pbm"))].low = 1.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("Globals"), S("MinaCalc.stream_pbm"))].low = 1.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("Globals"), S("MinaCalc.bad_newbie_skillsets_pbm"))].low = 1.0f;
#if DUMP_CONSTANTS == 0 // almost always
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("InlineConstants"), S("OHJ.h(81)"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("InlineConstants"), S("OHJ.h(89)"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("InlineConstants"), S("MinaCalc.cpp(76, 2)"))].low = 0.1f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("InlineConstants"), S("MinaCalc.cpp(94)"))].low = 0.1f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("InlineConstants"), S("MinaCalc.cpp(148)"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("InlineConstants"), S("MinaCalc.cpp(148, 2)"))].low = 0.0f;
    state.opt_cfg.bounds[param_index_by_name(&state.info, S("InlineConstants"), S("MinaCalc.cpp(536)"))].low = 0.0f;
#endif
    for (i32 i = 0; i < state.ps.num_params; i++) {
        state.opt_cfg.bounds[i].low /= normalization_factors[i];
        state.opt_cfg.bounds[i].high /= normalization_factors[i];
    }
    // Loss fine-tuning
    state.opt_cfg.barriers[0] = 2.0f;
    state.opt_cfg.barriers[1] = 2.0f;
    state.opt_cfg.barriers[2] = 2.0f;
    state.opt_cfg.barriers[3] = 2.0f;
    state.opt_cfg.barriers[4] = 2.0f;
    state.opt_cfg.barriers[5] = 2.0f;
    state.opt_cfg.barriers[6] = 2.0f;
    state.opt_cfg.barriers[7] = 2.0f;
    state.opt_cfg.goals[0] = 0.93f;
    state.opt_cfg.goals[1] = 0.93f;
    state.opt_cfg.goals[2] = 0.93f;
    state.opt_cfg.goals[3] = 0.93f;
    state.opt_cfg.goals[4] = 0.93f;
    state.opt_cfg.goals[5] = 0.93f;
    state.opt_cfg.goals[6] = 0.93f;
    state.opt_cfg.goals[7] = 0.93f;
    state.opt_cfg.bias[1].add = 0.0f; state.opt_cfg.bias[1].mul = 1.0f;
    state.opt_cfg.bias[2].add = 0.0f; state.opt_cfg.bias[2].mul = 1.0f;
    state.opt_cfg.bias[3].add = 0.0f; state.opt_cfg.bias[3].mul = 1.0f;
    state.opt_cfg.bias[4].add = 0.0f; state.opt_cfg.bias[4].mul = 1.0f;
    state.opt_cfg.bias[5].add = 0.0f; state.opt_cfg.bias[5].mul = 1.0f;
    state.opt_cfg.bias[6].add = 0.0f; state.opt_cfg.bias[6].mul = 1.0f;
    state.opt_cfg.bias[7].add = 0.0f; state.opt_cfg.bias[7].mul = 1.0f;
}

void optimizer_skulduggery(SimFileInfo *sfi, ParameterLossWork work, SkillsetRatings ssr)
{
    assert(state.target.msd_sd > 0.0f);
    i32 ss = sfi->target.skillset;
    f32 mean = state.target.msd_mean;
    f32 target = (work.msd - mean) / state.target.msd_sd;
    target = lerp(1.0f + target, expf(target), UnLogScale) - 1.0f;
    f32 skillset = (ssr.E[ss] - mean) / state.target.msd_sd;
    skillset = lerp(1.0f + skillset, expf(skillset), UnLogScale) - 1.0f;
    f32 overall = (ssr.overall - mean) / state.target.msd_sd;
    overall = lerp(1.0f + overall, expf(overall), UnLogScale) - 1.0f;
    f32 delta_skillset = skillset - target;
    f32 delta_overall = overall - target;
    f32 ssr_largest = 0;
    for (isize i = 1; i < NumSkillsets; i++) {
        ssr_largest = max(ssr_largest, ssr.E[i]);
    }
    f32 misclass = Misclass * expf(ssr_largest / ssr.E[ss] - 1.0f);
    f32 delta = sfi->target.weight * Scale * misclass * lerp(delta_skillset, delta_overall, SkillsetOverallBalance);
    i32 opt_param = state.opt_cfg.map.to_opt[work.param];
    f32 difference = 0.0f;
    if (work.param != Param_None) {
        difference = (work.value - state.info.defaults.params[work.param]) / state.opt_cfg.normalization_factors[work.param];
    }
    buf_push(state.opt_evaluations, (OptimizationEvaluation) {
        .sample = work.sample,
        .param = opt_param,
        .value_difference_from_initial = difference,
        .delta = delta,
        .barrier = state.opt_cfg.barriers[ss]
    });
    state.opt_pending_evals--;
    assert(state.opt_pending_evals >= 0);
}

void init(void)
{
    isize bignumber = 100*1024*1024;
    scratch_stack = stack_make(malloc(bignumber), bignumber);
    permanent_memory_stack = stack_make(malloc(bignumber), bignumber);

    push_allocator(scratch);
    Buffer font = {0}; //load_font_file("web/NotoSansCJKjp-Regular.otf");
    pop_allocator();

    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    stm_setup();
    simgui_setup(&(simgui_desc_t){
        .no_default_font = (font.buf != 0),
        .sample_count = _sapp.sample_count,
        .max_vertices = 65536*8,
    });
    state = (State) {
        .pass_action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f }
            }
        }
    };

    ImVec4 bg = *igGetStyleColorVec4(ImGuiCol_WindowBg);
    bg.w = 1.0f;
    igPushStyleColorVec4(ImGuiCol_WindowBg, bg);
    igPushStyleVarFloat(ImGuiStyleVar_ScrollbarSize, 4.f);
    igPushStyleVarFloat(ImGuiStyleVar_WindowRounding, 1.0f);
    igPushStyleVarVec2(ImGuiStyleVar_FramePadding, V2(8.0f, 4.0f));

    if (font.buf) {
        ImGuiIO* io = igGetIO();
        ImFontAtlas_AddFontFromMemoryTTF(io->Fonts, font.buf, (i32)font.len, 16.0f, 0, ImFontAtlas_GetGlyphRangesJapanese(io->Fonts));

        // Copy pasted from simgui_setup
        unsigned char* font_pixels;
        int font_width, font_height;
        int bytes_per_pixel;
        ImFontAtlas_GetTexDataAsRGBA32(io->Fonts, &font_pixels, &font_width, &font_height, &bytes_per_pixel);
        sg_image_desc img_desc;
        memset(&img_desc, 0, sizeof(img_desc));
        img_desc.width = font_width;
        img_desc.height = font_height;
        img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        img_desc.min_filter = SG_FILTER_LINEAR;
        img_desc.mag_filter = SG_FILTER_LINEAR;
        img_desc.content.subimage[0][0].ptr = font_pixels;
        img_desc.content.subimage[0][0].size = font_width * font_height * sizeof(uint32_t);
        img_desc.label = "sokol-imgui-font";
        _simgui.img = sg_make_image(&img_desc);
        io->Fonts->TexID = (ImTextureID)(uintptr_t) _simgui.img.id;
    }

    state.generation = 1;
    state.info = calc_info();
    state.calc = calc_init(&state.info);
    state.ps = copy_param_set(&state.info.defaults);
    buf_pushn(state.parameter_graphs_enabled, state.info.num_params);
    buf_reserve(state.graphs, 128);

    // todo: use handles instead
    buf_reserve(state.files, 1024);

    high_prio_work_queue.lock = make_lock();
    low_prio_work_queue.lock = make_lock();

    isize n_threads = maxs(1, got_any_cores() - 1);
    for (isize i = 0; i < n_threads; i++) {
        CalcThread *ct = buf_push(state.threads, (CalcThread) {
            .info = &state.info,
            .generation = &state.generation,
            .ps = &state.ps,
            .done = buf_pushn(done_queues, 1)
        });

        make_thread(calc_thread, ct);
    }

    for (i32 ss = 0; ss < NumSkillsets; ss++) {
        ImVec4 c = GetColormapColor(ss);
        c.w *= 0.9f;
        state.skillset_colors[ss] = igColorConvertFloat4ToU32(c);
        c.w *= 0.8f;
        state.skillset_colors_selectable[ss] = igColorConvertFloat4ToU32(c);
    }

    *null_sfi = (SimFileInfo) {
        .id = S("nullck"),
    };

    state.active = null_sfi;

    {
        ModInfo *mod = &state.info.mods[state.info.num_mods - 1];
        if (mod->num_params > 0) {
            i32 num_params = 0;
            i32 last_num_params = 0;
            char const *last_mod_file = file_for_param(&state.info, mod->index);
            char const *mod_file = 0;
            for (i32 i = 0; i < mod->num_params; i++) {
                i32 mp = mod->index + i;
                mod_file = file_for_param(&state.info, mp);
                if (strcmp(mod_file, last_mod_file) != 0) {
                    buf_push(state.inline_mods, (ModInfo) {
                        .name = after_last_slash(last_mod_file),
                        .num_params = num_params - last_num_params,
                        .index = mod->index + last_num_params,
                    });
                    last_mod_file = mod_file;
                    last_num_params = num_params;
                }
                num_params++;
            }
            buf_push(state.inline_mods, (ModInfo) {
                .name = after_last_slash(mod_file),
                .num_params = num_params - last_num_params,
                .index = mod->index + last_num_params,
            });
        }
    }

#if !defined(EMSCRIPTEN)
    add_target_files();
    setup_optimizer();
    checkpoint_default();
    checkpoint_latest();
    load_checkpoints_from_disk();
#endif
}

void frame(void)
{
    reset_scratch();
    finish_work();

    i32 width = sapp_width();
    i32 height = sapp_height();
    f64 delta_time = stm_sec(stm_laptime(&state.last_time));
    simgui_new_frame(width, height, delta_time);

    bool ss_highlight[NumSkillsets] = {0};
    SimFileInfo *next_active = 0;

    ImGuiIO *io = igGetIO();
    ImVec2 ds = io->DisplaySize;

    ImGuiWindowFlags fixed_window = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    SimFileInfo *active = state.active;

    f32 right_width = 400.0f;
    igSetNextWindowPos(V2(ds.x - right_width, 0.0f), ImGuiCond_Always, V2Zero);
    igSetNextWindowSize(V2(right_width + 1.0f, ds.y + 1.0f), ImGuiCond_Always);
    if (igBegin("Files", 0, fixed_window)) {
        // Header + arrow drop down fake thing
        if (state.target.average_delta.E[0] != 0.0f) {
            for (isize i = 0; i < NumSkillsets; i++) {
                igTextColored(msd_color(fabsf(state.target.average_delta.E[i]) * 3.0f), "%02.2f", (f64)state.target.average_delta.E[i]);
                tooltip("CalcTestList: %s abs delta", SkillsetNames[i]);
                if (i == 0) {
                    igSameLine(0, 0); igTextUnformatted(" (", 0); igSameLine(0, 0);
                    igTextColored(msd_color(fabsf(state.target.min_delta) * 3.0f), "%02.2f", (f64)state.target.min_delta);
                    tooltip("CalcTestList: min abs delta");
                    igSameLine(0, 0); igTextUnformatted(", ", 0); igSameLine(0, 0);
                    igTextColored(msd_color(fabsf(state.target.max_delta) * 3.0f), "%02.2f", (f64)state.target.max_delta);
                    tooltip("CalcTestList: max abs delta");
                    igSameLine(0, 0); igTextUnformatted(")", 0);
                }
                igSameLine(0, 12.0f);
            }
        }
        igSameLine(igGetWindowWidth() - 36.0f, 4);
        b32 skillsets = false;
        igPushStyleColorU32(ImGuiCol_HeaderHovered, 0);
        igPushStyleColorU32(ImGuiCol_HeaderActive, 0);
        if (igTreeNodeExStr("##skillsets toggle", ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
            skillsets = true;
        } tooltip("Skillsets");
        igPopStyleColor(2);
        igSeparator();

        // Open file list
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            FnGraph *g = &state.graphs[sfi->graphs[0]];
            f32 wife93 = sfi->aa_rating;
            f32 wife965 = sfi->max_rating;
            if (sfi->target.want_msd) {
                igTextColored(msd_color(fabsf(sfi->target.delta) * 3.0f), "%s%02.2f", sfi->target.delta >= 0.0f ? " " : "", (f64)sfi->target.delta);
                tooltip("CalcTestList: %.2f %s, want %.2f at %.2fx", (f64)sfi->target.got_msd, SkillsetNames[sfi->target.skillset], (f64)sfi->target.want_msd, (f64)sfi->target.rate); igSameLine(0, 7.0f);
            }
            igTextColored(msd_color(wife93), "%2.2f", (f64)wife93);
            tooltip("Overall at AA"); igSameLine(0, 7.0f);
            igTextColored(msd_color(wife965), "%2.2f", (f64)wife965);
            tooltip("Overall at max scaling"); igSameLine(0, 7.0f);
            igPushIDStr(sfi->id.buf);
            if (sfi == active) {
                igPushStyleColorVec4(ImGuiCol_Header, msd_color(sfi->aa_rating));
            }
            if (igSelectableBool(sfi->id.buf, sfi->open, ImGuiSelectableFlags_None, V2Zero)) {
                if (sfi->open) {
                    sfi->open = false;
                } else {
                    sfi->open = true;
                    next_active = sfi;
                }
            }
            if (sfi == active) {
                igPopStyleColor(1);
            }
            if (sfi->stops) {
                igSameLine(igGetWindowWidth() - 30.f, 4);
                igTextColored((ImVec4) { 0.85f, 0.85f, 0.0f, 0.95f }, "  !  ");
                tooltip("This file has stops or negBPMs. These are not parsed in the same way as Etterna, so the ratings will differ from what you see in game.\n\n"
                        "Note that the calc is VERY sensitive to tiny variations in ms row times, even if the chartkeys match.");
            }
            if (skillsets) {
                for (isize ss = 0; ss < NumSkillsets; ss++) {
                    char r[32] = {0};
                    snprintf(r, sizeof(r), "%2.2f##%d", (f64)g->ys[ss][Wife930Index], (i32)ss);
                    igPushStyleColorU32(ImGuiCol_Header, state.skillset_colors_selectable[ss]);
                    igPushStyleColorU32(ImGuiCol_HeaderHovered, state.skillset_colors[ss]);
                    igSelectableBool(r, sfi->display_skillsets[ss], ImGuiSelectableFlags_None, V2(300.0f / NumSkillsets, 0));
                    tooltip("%s", SkillsetNames[ss]);
                    if (igIsItemHovered(0)) {
                        ss_highlight[ss] = 1;
                    }
                    igPopStyleColor(2);
                    igSameLine(0, 4);
                    igDummy(V2(4,0));
                    igSameLine(0, 4);
                }
                igNewLine();
            }
            igPopID();
            igSeparator();
        }
    }
    igEnd();

    ParamSliderChange changed_param = {0};

    f32 left_width = 302.0f;
    igSetNextWindowPos(V2(-1.0f, 0), ImGuiCond_Always,  V2Zero);
    igSetNextWindowSize(V2(left_width + 1.0f, ds.y + 1.0f), ImGuiCond_Always);
    if (igBegin("Mod Parameters", 0, fixed_window)) {
        u32 effect_mask = 0;
        isize clear_selections_to = -1;
        igPushIDStr(active->id.buf);

        // Skillset filters
        f32 selectable_width_factor = 4.0f;
        for (isize i = 0; i < NumSkillsets; i++) {
            igPushStyleColorU32(ImGuiCol_Header, state.skillset_colors_selectable[i]);
            igPushStyleColorU32(ImGuiCol_HeaderHovered, state.skillset_colors[i]);
            igSelectableBoolPtr(SkillsetNames[i], &active->selected_skillsets[i], 0, V2((igGetWindowWidth() - 12*4) / selectable_width_factor, 0));
            igPopStyleColor(2);
            if (ItemDoubleClicked(0)) {
                clear_selections_to = i;
            }
            if (igIsItemHovered(0)) {
                ss_highlight[i] = 1;
            }
            if (i != 3) {
                igSameLine(0, 4);
                igDummy(V2(4, 0));
                igSameLine(0, 4);
            }
            effect_mask |= (active->selected_skillsets[i] << i);
        }
        igPopID();
        if (clear_selections_to >= 0) {
            memset(active->selected_skillsets, 0, sizeof(active->selected_skillsets));
            active->selected_skillsets[clear_selections_to] = 1;
        }
        igNewLine();

        // Tabs for param strength filters
        if (igBeginTabBar("FilterTabs", ImGuiTabBarFlags_NoTooltip)) {
            if (igBeginTabItem("Relevant", 0, ImGuiTabItemFlags_None)) {
                tooltip("More, plus some params that need more shoving");
                for (i32 i = 0; i < state.info.num_mods - 1; i++) {
                    mod_param_sliders(&state.info.mods[i], i, active->effects.weak, effect_mask, &changed_param);
                }
                inlines_param_sliders((i32)state.info.num_mods - 1, active->effects.weak, effect_mask, &changed_param);
                igEndTabItem();
            } else tooltip("More, plus some params that need more shoving");
            if (igBeginTabItem("More Relevant", 0, ImGuiTabItemFlags_None)) {
                tooltip("These will basically always change the MSD of the active file's selected skillsets");
                for (i32 i = 0; i < state.info.num_mods - 1; i++) {
                    mod_param_sliders(&state.info.mods[i], i,  active->effects.strong, effect_mask, &changed_param);
                }
                inlines_param_sliders((i32)state.info.num_mods - 1, active->effects.strong, effect_mask, &changed_param);
                igEndTabItem();
            } else tooltip("These will basically always change the MSD of the active file's selected skillsets");
            if (igBeginTabItem("All", 0, ImGuiTabItemFlags_None)) {
                tooltip("Everything\nPretty useless unless you like finding out which knobs do nothing yourself");
                for (i32 i = 0; i < state.info.num_mods - 1; i++) {
                    mod_param_sliders(&state.info.mods[i], i,  0, 0, &changed_param);
                }
                inlines_param_sliders((i32)state.info.num_mods - 1,  0, 0, &changed_param);
                igEndTabItem();
            } else tooltip("Everything\nPretty useless unless you like finding out which knobs do nothing yourself");
        }
        igEndTabBar();
    }
    igEnd();

    if (changed_param.type == ParamSlider_ValueChanged || changed_param.type == ParamSlider_OptimizingToggled) {
        state.generation++;
        buf_clear(state.high_prio_work);
        buf_clear(state.low_prio_work);
    }

    // MSD graph windows
    i32 num_open_windows = 0;
    f32 centre_width = ds.x - left_width - right_width;
    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
        if (sfi->open) {
            // Window placement
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
            if ((state.last_frame.num_open_windows == 0 && centre_width >= 450.0f) || sfi->fullscreen_window) {
                sfi->fullscreen_window = true;
                window_flags = ImGuiWindowFlags_NoResize;
                igSetNextWindowPos(V2(left_width, 0), ImGuiCond_Always, V2Zero);
                igSetNextWindowSize(V2(centre_width, ds.y), ImGuiCond_Always);
            } else {
                sfi->fullscreen_window = false;
                ImVec2 sz = V2(clamp_high(ds.x, 750.0f), clamp(300.0f, ds.y, ds.y * 0.33f * (f32)buf_len(sfi->graphs)));
                ImVec2 pos = V2(left_width + rngf() * (ds.x - left_width - sz.x), rngf() * (ds.y - sz.y));
                igSetNextWindowPos(pos, ImGuiCond_Once, V2Zero);
                igSetNextWindowSize(sz, ImGuiCond_Once);
            }
            if (igBegin(sfi->id.buf, &sfi->open, window_flags)) {
                calculate_effects(sfi == active ? &state.high_prio_work : &state.low_prio_work, &state.info, sfi, false, state.generation);

                if (igIsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                    next_active = sfi;
                }

                // Unset fullscreen window on drag
                if (igIsWindowHovered(ImGuiHoveredFlags_RootWindow) && igIsMouseDragging(0, -1.f)) {
                    sfi->fullscreen_window = false;
                }

                // File difficulty + chartkey text
                igTextColored(msd_color(sfi->aa_rating), "%.2f", (f64)sfi->aa_rating);
                igSameLine(0, 4);
                igText("(Want %g at %gx)", (f64)sfi->target.want_msd, (f64)sfi->target.rate);
                igSameLine(clamp_low(GetContentRegionAvailWidth() - 235.f, 100), 0);
                igText(sfi->chartkey.buf);

                igSetNextItemWidth(36.0f);
                igDragFloat("Target MSD Bias", &sfi->target.msd_bias, 0.05f, -sfi->target.want_msd, 40.0f - sfi->target.want_msd, "%g", 1.0f);
                igSetNextItemWidth(36.0f);
                igDragFloat("Target Weight", &sfi->target.weight, 0.05f, 0.0f, 10000.0f, "%g", 1.0f);

                // Plots. Weird rendering order: first 0, then backwards from the end
                FnGraph *fng = &state.graphs[sfi->graphs[0]];
                i32 zoomable = fng->initialised && fng->zoomable_once ? ImGuiCond_Once : ImGuiCond_Always;
                if (fng->initialised && !fng->zoomable_once) {
                    fng->zoomable_once = true;
                }
                ipSetNextPlotLimits((f64)WifeXs[0] * 100, (f64)WifeXs[Wife965Index + 1] * 100, (f64)fng->min - 1.0, (f64)fng->max + 2.0, zoomable);
                if (BeginPlotDefaults("Rating", "Wife%", "SSR")) {
                    calculate_file_graph(&state.high_prio_work, sfi, fng, state.generation);
                    for (i32 ss = 0; ss < NumSkillsets; ss++) {
                        skillset_line_plot(ss, ss_highlight[ss], fng, fng->ys[ss]);
                    }
                    ipEndPlot();
                }
                igSameLine(0, 4);
                ipSetNextPlotLimits((f64)WifeXs[0] * 100, (f64)WifeXs[Wife965Index + 1] * 100, (f64)fng->relative_min - 0.05, 1.05, zoomable);
                if (BeginPlotDefaults("Relative Rating", "Wife%", 0)) {
                    for (i32 ss = 1; ss < NumSkillsets; ss++) {
                        skillset_line_plot(ss, ss_highlight[ss], fng, fng->relative_ys[ss]);
                    }
                    ipEndPlot();
                }

                for (isize fungi = buf_len(sfi->graphs) - 1; fungi >= 1; fungi--) {
                    fng = &state.graphs[sfi->graphs[fungi]];
                    zoomable = fng->initialised && fng->zoomable_once ? ImGuiCond_Once : ImGuiCond_Always;
                    if (fng->initialised && !fng->zoomable_once) {
                        fng->zoomable_once = true;
                    }
                    if (fng->param == changed_param.param && (changed_param.type == ParamSlider_LowerBoundChanged || changed_param.type == ParamSlider_UpperBoundChanged)) {
                        calculate_parameter_graph_force(&state.high_prio_work, sfi, fng, state.generation);
                        zoomable = ImGuiCond_Always;
                    }

                    i32 mp = fng->param;
                    ParamInfo *p = &state.info.params[mp];
                    u8 full_name[64] = {0};
                    snprintf(full_name, sizeof(full_name), "%s.%s", state.info.mods[p->mod].name, p->name);
                    igPushIDStr(full_name);
                    ipSetNextPlotLimits((f64)state.ps.min[mp], (f64)state.ps.max[mp], (f64)fng->min - 1.0, (f64)fng->max + 2.0, zoomable);
                    if (BeginPlotDefaults("##AA", full_name, "AA Rating")) {
                        calculate_parameter_graph(&state.high_prio_work, sfi, fng, state.generation);
                        for (i32 ss = 0; ss < NumSkillsets; ss++) {
                            skillset_line_plot(ss, ss_highlight[ss], fng, fng->ys[ss]);
                        }
                        ipEndPlot();
                    }
                    igSameLine(0, 4);
                    ipSetNextPlotLimits((f64)state.ps.min[mp], (f64)state.ps.max[mp], (f64)fng->relative_min - 0.05, (f64)1.05, zoomable);
                    if (BeginPlotDefaults("##Relative AA", full_name, 0)) {
                        for (i32 ss = 1; ss < NumSkillsets; ss++) {
                            skillset_line_plot(ss, ss_highlight[ss], fng, fng->relative_ys[ss]);
                        }
                        ipEndPlot();
                    }
                    igPopID();
                }
            }
            igEnd();
            num_open_windows++;
        }
    }

    // Optimization window
    if (state.loss_window && buf_len(state.files) > 0) {
        static const char *error_message = 0;
        if (!state.optimization_graph) {
            state.optimization_graph = &state.graphs[make_optimization_graph()];
        }

        if (state.optimizing) {
            static u32 last_generation = 1;
            if (last_generation != state.generation) {
                // Some parameter has been messed with, or turned off
                // This invalidates the optimizer state either way, so just recreate the whole thing
                buf_clear(state.opt_evaluations);

                f32 *x = 0;
                push_allocator(scratch);
                buf_pushn(x, state.ps.num_params);
                pop_allocator();

                i32 n_params = pack_opt_parameters(&state.info, state.ps.params, state.opt_cfg.normalization_factors, state.opt_cfg.enabled, x, state.opt_cfg.map.to_ps, state.opt_cfg.map.to_opt);
                i32 iter = state.opt.iter - 1;
                state.opt = optimize(n_params, x, (i32)buf_len(state.files));
                state.opt.iter = iter;

                last_generation = state.generation;
                state.opt_pending_evals = 0;
            }

            if (state.opt_pending_evals == 0) {
                state.generation++;
                last_generation = state.generation;

                OptimizationRequest req = opt_pump(&state.opt, state.opt_evaluations);
                assert(req.n_samples > 0);
                buf_clear(state.opt_evaluations);

                for (isize i = 0; i < state.opt.n_params; i++) {
                    if (isnan(state.opt.x[i])) {
                        state.optimizing = false;
                        error_message = "Cannot optimize due to NaNs. Reset to a good state.";
                        break;
                    }
                }

                // Do this even if we have NaNs so that they propagate to the UI.
                for (isize i = 0; i < state.opt.n_params; i++) {
                    isize p = state.opt_cfg.map.to_ps[i];
                    // Apply bounds. This is a kind of a hack so it lives out here and not in the optimizer :)
                    state.opt.x[i] = clamp(state.opt_cfg.bounds[p].low, state.opt_cfg.bounds[p].high, state.opt.x[i]);
                    // Copy x into the calc parameters
                    state.ps.params[p] = state.opt.x[i] * state.opt_cfg.normalization_factors[p];
                }

                if (state.optimizing) {
                    if (!error_message) {
                        checkpoint_latest();
                    }
                    error_message = 0;

                    i32 submitted = 0;
                    for (isize i = 0; i < req.n_samples; i++) {
                        i32 sample = state.opt.active.samples[i];
                        i32 file_index = sample % buf_len(state.files);
                        assert(file_index >= 0);
                        SimFileInfo *sfi = &state.files[file_index];

                        if (sfi->target.weight == 0.0f) {
                            req.n_samples = req.n_samples < buf_len(state.files) ? req.n_samples + 1 : req.n_samples;
                            continue;
                        }

                        i32 ss = sfi->target.skillset;
                        f32 goal = state.opt_cfg.goals[ss];
                        f32 msd = (sfi->target.want_msd + sfi->target.msd_bias + state.opt_cfg.bias[ss].add) * state.opt_cfg.bias[ss].mul;

                        i32 params_submitted = 0;
                        for (isize j = 0; j < state.opt.n_params; j++) {
                            i32 param = state.opt.active.parameters[j];
                            i32 p = state.opt_cfg.map.to_ps[param];
                            if (sfi->num_effects_computed == 0 || sfi->effects.weak[p]) {
                                calculate_parameter_loss(&state.low_prio_work, sfi, (ParameterLossWork) {
                                    .sample = sample,
                                    .param = p,
                                    .value = (state.opt.x[param] + req.h) * state.opt_cfg.normalization_factors[p],
                                    .goal = goal,
                                    .msd = msd
                                }, state.generation);
                                params_submitted++;
                            }

                            if (params_submitted == req.n_parameters) {
                                break;
                            }
                        }

                        if (params_submitted > 0) {
                            ParameterLossWork plw_x = { .sample = sample, .goal = goal, .msd = msd };
                            calculate_parameter_loss(&state.low_prio_work, sfi, plw_x, state.generation);
                            submitted++;

                            submitted += params_submitted;
                        }

                        if (buf_len(state.high_prio_work) == 0 && (sfi->num_effects_computed == 0 || ((state.generation - sfi->effects_generation) > 1024))) {
                            sfi->num_effects_computed = 0;
                            calculate_effects(&state.high_prio_work, &state.info, sfi, true, state.generation);
                        }
                    }

                    state.opt_pending_evals = submitted;

                    static u64 t = 0;
                    if (stm_sec(stm_since(t)) > 1.0) {
                        t = stm_now();

                        for (isize i = 0; i < buf_len(state.files); i++) {
                            calculate_skillsets(&state.high_prio_work, &state.files[i], false, state.generation);
                        }
                    }

                    i32 min_idx = 0;
                    i32 max_idx = 0;
                    for (i32 i = 1; i < buf_len(state.files); i++) {
                        if (state.files[i].target.delta > state.files[max_idx].target.delta) {
                            max_idx = i;
                        }
                        if (state.files[i].target.delta < state.files[min_idx].target.delta) {
                            min_idx = i;
                        }
                    }

                    opt_focus(&state.opt, max_idx);
                    opt_focus(&state.opt, min_idx);

                    state.optimization_graph->ys[0][state.opt.iter % NumGraphSamples] = fabsf(state.target.average_delta.E[0]);
                    state.optimization_graph->ys[1][state.opt.iter % NumGraphSamples] = state.opt.loss;
                }
            }
        }
        igSetNextWindowPos(V2(left_width, 0), ImGuiCond_Always, V2Zero);
        igSetNextWindowSize(V2(centre_width, ds.y - 50.f), ImGuiCond_Always);
        if (igBegin("Loss", 0, ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoResize)) {
            num_open_windows++;
            f32 err_lim = 2.0f;
            f32 loss_lim = FLT_MIN;
            for (isize i = 0; i < NumGraphSamples; i++) {
                err_lim = max(err_lim, state.optimization_graph->ys[0][i]);
                loss_lim = max(loss_lim, state.optimization_graph->ys[1][i]);
            }
            ipPushStyleColorU32(ImPlotCol_Line, state.skillset_colors[0]);
            ipSetNextPlotLimits(0, NumGraphSamples, 0, (f64)err_lim, ImGuiCond_Always);
            if (BeginPlotDefaultsOptimizer("##Average Error", "", "Average Error")) {
                ipPlotLineFloatPtrInt("Error", state.optimization_graph->ys[0], state.optimization_graph->len, 0, sizeof(float));
                ipEndPlot();
            }
            ipSetNextPlotLimits(0, NumGraphSamples, 0, (f64)loss_lim, ImGuiCond_Always);
            if (BeginPlotDefaultsOptimizer("##Loss", "Iteration", "Loss")) {
                ipPushStyleColorU32(ImPlotCol_Line, state.skillset_colors[0]);
                ipPlotLineFloatPtrInt("Loss", state.optimization_graph->ys[1], state.optimization_graph->len, 0, sizeof(float));
                ipEndPlot();
            }
            ipPopStyleColor(1);
        }

        if (igButton(state.optimizing ? "Stop" : "Start", V2Zero)) {
            state.optimizing = !state.optimizing;
        } igSameLine(0, 4);
        if (igButton("Checkpoint", V2Zero)) {
            checkpoint();
        }
        if (error_message) {
            igSameLine(0, 4);
            igTextUnformatted(error_message, 0);
        }

        if (igBeginChildStr("Hyperparameters", V2(GetContentRegionAvailWidth() / 2.0f, 250.0f), 0, 0)) {
            igSliderFloat("h", &H, 1.0e-8f, 0.1f, "%g", 1.0f);
            tooltip("coarseness of the derivative approximation\n\nfinite differences baybee");
            igSliderFloat("Step Size", &StepSize, 1.0e-8f, 0.1f, "%g", 1.0f);
            tooltip("how fast to change parameters. large values can be erratic");
            // igSliderFloat("MDecay", &MDecay, 0.0f, 1.0f - 1e-3f, "%g", 0.5f);
            // igSliderFloat("VDecay", &VDecay, 0.0f, 1.0f - 1e-5f, "%g", 0.5f);
            igSliderInt("Sample Batch Size", &SampleBatchSize, 1, state.opt.n_samples, "%d");
            tooltip("random sample of n files for each step");
            igSliderInt("Parameter Batch Size", &ParameterBatchSize, 1, state.opt.n_params, "%d");
            tooltip("random sample of n parameters for each step");
            igSliderFloat("Skillset/Overall Balance", &SkillsetOverallBalance, 0.0f, 1.0f, "%f", 1.0f);
            tooltip("0 = train only on skillset\n1 = train only on overall");
            igSliderFloat("Misclass Penalty", &Misclass, 0.0f, 5.0f, "%f", 1.0f);
            tooltip("exponentially increases loss proportional to (largest_skillset_ssr - target_skillset_ssr)");
            // igSliderFloat("Scale", &Scale, 0.1f, 20.0f, "%f", 1.0f);
            igSliderFloat("Exp Scale", &UnLogScale, 0.0f, 1.0f, "%f", 1.0f);
            tooltip("weights higher MSDs heavier automatically");
            igSliderFloat("Underrated dead zone", &NegativeEpsilon, 0.0f, 10.0f, "%f", 1.0f);
            tooltip("be more accepting of files that come under their target ssr than over");
            igSliderFloat("Regularisation", &Regularisation, 0.0f, 1.0f, "%f", 2.f);
            tooltip("penalise moving parameters very far away from the defaults");
            igSliderFloat("Regularisation Alpha", &RegularisationAlpha, 0.0f, 1.0f, "%f", 1.0f);
            tooltip("0 = prefer large changes to few parameters\n1 = prefer small changes to many parameters\n...theoretically");
        }
        igEndChild();
        igSameLine(0, 4);


        if (igBeginChildStr("Checkpoints", V2(GetContentRegionAvailWidth(), 250.0f), 0, 0)) {
            if (igSelectableBool(state.latest_checkpoint.name, 0, ImGuiSelectableFlags_None, V2Zero)) {
                restore_checkpoint(state.latest_checkpoint);
            }
            if (igSelectableBool(state.default_checkpoint.name, 0, ImGuiSelectableFlags_None, V2Zero)) {
                restore_checkpoint(state.default_checkpoint);
            }
            for (isize i = buf_len(state.checkpoints) - 1; i >= 0; i--) {
                if (igSelectableBool(state.checkpoints[i].name, 0, ImGuiSelectableFlags_None, V2Zero)) {
                    restore_checkpoint(state.checkpoints[i]);
                }
            }
        }
        igEndChild();
        for (isize i = 1; i < NumSkillsets; i++) {
            igPushIDInt((i32)i);
            igText("%s Bias", SkillsetNames[i]);
            tooltip("+ offsets the target MSD for this skillset\n* multiplies\n^ sharpens the squared loss for positive error\n%% sets the target wife%%");
            igSameLine(0, 4);
            igSetCursorPosX(116.f);
            igTextUnformatted("+", 0);
            igSameLine(0,4);
            igSetNextItemWidth(igGetWindowContentRegionWidth() / 5.0f);
            igSliderFloat("*##+", &state.opt_cfg.bias[i].add, -5.0f, 5.0f, "%f", 1.0f);
            igSameLine(0, 4);
            igSetNextItemWidth(igGetWindowContentRegionWidth() / 5.0f);
            igSliderFloat("^##*", &state.opt_cfg.bias[i].mul, 0.1f, 5.0f, "%f", 1.0f);
            igSameLine(0, 4);
            igSetNextItemWidth(igGetWindowContentRegionWidth() / 5.0f);
            igSliderFloat("%##^", &state.opt_cfg.barriers[i], 2.0f, 8.0f, "%f", 1.0f);
            igSameLine(0, 4);
            igSetNextItemWidth(igGetWindowContentRegionWidth() / 5.0f);
            igSliderFloat("##%", &state.opt_cfg.goals[i], 0.9f, 0.965f, "%f", 1.0f);
            igPopID();
        }

        igEnd();
    }

    // Start up text window
    if (buf_len(state.files) == 0) {
        igSetNextWindowPos(V2(left_width + centre_width / 2.0f, ds.y / 2.f), 0, V2(0.5f, 0.5f));

        igBegin("Drop", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        igText("Unable to load any files.\n\n");
        igText("Place the .exe next to a cache.db and CalcTestList.xml, or run on the command line like:\n\n");
        igText("seeminacalc db=/etterna/Cache/cache.db list=/path/to/CalcTestList.xml");
        igEnd();
#ifdef NO_SSE
        igSetNextWindowPos(V2(left_width + centre_width / 2.0f, ds.y / 2.f + 20.f), 0, V2(0.5f, 0.0f));
        igBegin("SSE", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        igTextColored((ImVec4) { 1, 0, 0, 1.0f}, "No SSE available");
        igText("Turn on SIMD support in about:flags if you want the calc to be exactly the same as in game");
        igTextColored((ImVec4) { 1, 1, 1, 0.5f}, "(this is a numerical precision thing, not a speed thing)");
        igEnd();
#endif
    }

    // Debug window
    if (igIsKeyPressed('`', false)) {
        state.debug_window = !state.debug_window;

        DUMP_CONSTANT_INFO;
    }
    if (!state.debug_window) {
        debug_counters.skipped = 0;
        debug_counters.done = 0;
        f64 time = 0;
        for (CalcThread *ct = state.threads; ct != buf_end(state.threads); ct++) {
            debug_counters.skipped += ct->debug_counters.skipped;
            debug_counters.done += ct->debug_counters.done;
            time += ct->debug_counters.time;
        }
        time = 1000. * time / (f64)buf_len(state.threads);

        igSetNextWindowSize(V2(centre_width - 5.0f, 0), 0);
        igSetNextWindowPos(V2(left_width + 2.f, ds.y - 2.f), 0, V2(0, 1));
        igBegin("Debug", &state.debug_window, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

        igBeginGroup(); {
            isize used = permanent_memory->ptr - permanent_memory->buf;
            igText("Calc version %d (%d mods, %d params)", state.info.version, state.info.num_mods, state.info.num_params - 1);
            igText("Using %_$d / %_$d", used - total_bytes_leaked, used);
        } igEndGroup();
        igSameLine(0, 45.f);
        igText("Calc calls ");
        igSameLine(0, 0.f);
        igBeginGroup(); igText("requested"); igText("%_$d", debug_counters.requested); igEndGroup(); igSameLine(0, 4);
        igBeginGroup(); igText("skipped"); igText("%_$d", debug_counters.skipped); igEndGroup(); igSameLine(0, 4);
        igBeginGroup(); igText("done"); igText("%_$d", debug_counters.done); igEndGroup(); igSameLine(0, 4);
        igBeginGroup(); igText("pending"); igText("%_$d", debug_counters.requested - (debug_counters.done + debug_counters.skipped)); igEndGroup(); igSameLine(0, 4);
        igSameLine(0, 45.f);
        igBeginGroup(); igText("Average calc time per thousand non-empty rows"); igText("%.2fms", time); igEndGroup();

        igEnd();
    }

    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();

    state.last_frame.num_open_windows = num_open_windows;
    state.last_frame.left_width = left_width;
    state.last_frame.centre_width = centre_width;
    state.last_frame.right_width = right_width;

    // Restore last interacted-with window on close
    if (active->open == false) {
        next_active = null_sfi;
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (sfi->open && sfi->frame_last_focused >= next_active->frame_last_focused) {
                next_active = sfi;
            }
        }
        if (next_active == null_sfi) {
            next_active = 0;
        }
    }

    if (next_active) {
        assert(next_active->open);
        next_active->frame_last_focused = _sapp.frame_count;
        state.active = next_active;
        calculate_file_graphs(&state.high_prio_work, state.active, state.generation);
    }

    if (changed_param.type) {
        switch (changed_param.type) {
            case ParamSlider_GraphToggled: {
                if (state.parameter_graphs_enabled[changed_param.param]) {
                    buf_push(state.parameter_graph_order, changed_param.param);
                    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                        buf_push(sfi->graphs, make_parameter_graph(changed_param.param));
                    }
                } else {
                    for (isize i = 0; i < buf_len(state.parameter_graph_order); i++) {
                        if (state.parameter_graph_order[i] == changed_param.param) {
                            buf_remove_sorted_index(state.parameter_graph_order, i);
                            break;
                        }
                    }

                    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                        for (isize i = 0; i != buf_len(sfi->graphs); i++) {
                            if (state.graphs[sfi->graphs[i]].param == changed_param.param) {
                                free_graph(sfi->graphs[i]);
                                buf_remove_sorted_index(sfi->graphs, i);
                                break;
                            }
                        }
                    }
                }
            } break;
            case ParamSlider_ValueChanged: {
                calculate_skillsets_in_background(&state.low_prio_work, state.generation);
            } break;
            default: {
                // Nothing
            }
        }
    }

    // Randomise submission order for the high prio queue
    // This lets you sweep param sliders without the first graphs stealing all the work
    for (isize i = 0; i < buf_len(state.high_prio_work); i++) {
        isize idx = i + (rng() % (buf_len(state.high_prio_work) - i));
        CalcWork temp = state.high_prio_work[i];
        state.high_prio_work[i] = state.high_prio_work[idx];
        state.high_prio_work[idx] = temp;
    }

    submit_work(&high_prio_work_queue, state.high_prio_work, state.generation);
    submit_work(&low_prio_work_queue, state.low_prio_work, state.generation);
    fflush(0);
}

void cleanup(void)
{
    simgui_shutdown();
    sg_shutdown();
}

void input(const sapp_event* event)
{
    simgui_handle_event(event);
}

sapp_desc sokol_main(int argc, char* argv[])
{
    sargs_setup(&(sargs_desc){
        .argc = argc,
        .argv = argv
    });

    db_path = sargs_value_def("db", "cache.db");
    test_list_path = sargs_value_def("list", "CalcTestList.xml");

    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 1920,
        .height = 1080,
        .sample_count = 8,
        .window_title = "SeeMinaCalc",
        .ios_keyboard_resizes_canvas = false
    };
}
