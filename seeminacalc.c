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

    ParamSet ps;
    bool *parameter_graphs_enabled;
    i32 *parameter_graph_order;
    FnGraph *graphs;
    i32 *free_graphs;

    OptimizationContext opt;
    OptimizationEvaluation *opt_evaluations;
    i32 opt_pending_evals;
    FnGraph *optimization_graph;
    struct {
        f32 barriers[NumSkillsets];
        f32 *normalization_factor;
        Bound *bounds;
        struct {
            i32 *to_opt;
            i32 *to_ps;
        } map;
    } opt_cfg;

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
        b32 show_parameter_names;
        f32 left_width;
        f32 centre_width;
        f32 right_width;
    } last_frame;

    b8 debug_window;
    b8 loss_window;
} State;
static State state = {0};

#include "cachedb.gen.c"
#include "graphs.c"

#pragma float_control(precise, on, push)
#include "sm.c"
#pragma float_control(pop)

static const ImVec2 V2Zero = {0};

bool BeginPlotCppCpp(const char* title, const char* x_label, const char* y_label, const ImVec2* size, ImPlotFlags flags, ImPlotAxisFlags x_flags, ImPlotAxisFlags y_flags, ImPlotAxisFlags y2_flags, ImPlotAxisFlags y3_flags);
static bool BeginPlotDefaults(const char* title_id, const char* x_label, const char* y_label)
{
    return BeginPlotCppCpp(title_id, x_label, y_label, &(ImVec2){igGetWindowWidth() / 2.0f - 8.0f, 0}, ImPlotFlags_Default & ~ImPlotFlags_Legend, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary);
}
static bool BeginPlotDefaultsFullWidth(const char* title_id, const char* x_label, const char* y_label)
{
    return BeginPlotCppCpp(title_id, x_label, y_label, &(ImVec2){igGetWindowWidth() - 8.0f, 0}, ImPlotFlags_Default & ~ImPlotFlags_Legend, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary);
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
    if (buf_len(state.files) + array_length(TargetFiles) == buf_cap(state.files)) {
        printf("please.. no more files");
        return -1;
    }

    push_allocator(scratch);
    NoteInfo *ni = 0;
    buf_reserve(ni, 40000);
    pop_allocator();

    f32 target_m = TargetFiles[0].target;
    f32 target_s = 0.0f;

    for (isize i = 0; i < array_length(TargetFiles); i++) {
        push_allocator(scratch);
        TargetFile *target = &TargetFiles[i];
        String ck = target->key;
        String title = target->title;
        String author = target->author;
        String diff = SmDifficultyStrings[target->difficulty];
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

        buf_clear(ni);
        for (isize k = 0; k < target->note_data_len; k++) {
            buf_push(ni, (NoteInfo) { .notes = target->note_data_notes[k], .rowTime = target->note_data_times[k] });
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
            .notes_len = (i32)buf_len(ni),
            .notes = frobble_note_data(ni, buf_len(ni)),
            .selected_skillsets[0] = true,
        });

        buf_push(sfi->graphs, make_skillsets_graph());

        buf_reserve(sfi->effects.weak, state.info.num_params);
        buf_reserve(sfi->effects.strong, state.info.num_params);

        calculate_skillsets(&state.high_prio_work, sfi, true, state.generation);

        if (i > 0) {
            f32 old_m = target_m;
            f32 old_s = target_s;
            target_m = old_m + (target->target - old_m) / (i + 1);
            target_s = old_s + (target->target - old_m)*(target->target - target_m);
        }

        printf("Added %s\n", id.buf);
    }

    f32 target_var = target_s / (array_length(TargetFiles) - 1);
    state.target.msd_mean = target_m;
    state.target.msd_sd = sqrtf(target_var);

    submit_work(&high_prio_work_queue, state.high_prio_work, state.generation);

    state.loss_window = true;

    return 0;
}

enum {
    ParamSlider_Nothing,
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

static void param_slider_widget(i32 param_idx, b32 show_parameter_names, ParamSliderChange *out)
{
    assert(out);
    i32 type = ParamSlider_Nothing;
    f32 value = 0.0f;
    i32 mp = param_idx;
    igPushIDInt(mp);
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
        f32 speed = (state.ps.max[mp] - state.ps.min[mp]) / 100.f;
        igSetNextItemWidth(igGetFontSize() * 10.0f);
        if (igSliderFloat(slider_id, &state.ps.params[mp], state.ps.min[mp], state.ps.max[mp], "%f", 1.0f)) {
            type = ParamSlider_ValueChanged;
            value = state.ps.params[mp];
        }
        if (show_parameter_names == false) {
            tooltip(state.info.params[mp].name);
        }
        if (ItemDoubleClicked(0)) {
            state.ps.params[mp] = state.info.defaults.params[mp];
            type = ParamSlider_ValueChanged;
            value = state.ps.params[mp];
        }
        igSameLine(0, 4);

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
    }
    if (show_parameter_names) {
        igSameLine(0, 0);
        igText(state.info.params[mp].name);
    }
    igPopID();
    if (out && type != ParamSlider_Nothing) {
        assert(out->type == ParamSlider_Nothing);
        out->type = type;
        out->param = mp;
        out->value = value;
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

isize param_opt_index_by_name(CalcInfo *info, i32 *map, String mod, String param)
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
    return map[idx];
}

void setup_optimizer(void)
{
    f32 *normalization_factors = 0;
    #if 0
    f32 *initial_x = 0;
    push_allocator(scratch);
    buf_pushn(initial_x, state.ps.num_params);
    pop_allocator();
    #else
    #include "x3.txt"
    #endif

    buf_pushn(normalization_factors, state.ps.num_params);
    buf_pushn(state.opt_cfg.map.to_ps, state.ps.num_params + 1);
    // This sets to_ps[-1] = to_ps[Param_None] = 0
    state.opt_cfg.map.to_ps = state.opt_cfg.map.to_ps + 1;
    buf_pushn(state.opt_cfg.map.to_opt, state.ps.num_params);
    i32 n_params = 0;
    // The param at 0, rate, is not a real parameter
    state.opt_cfg.map.to_opt[0] = Param_None;
    for (i32 i = 1; i < state.ps.num_params; i++) {
        if (state.info.params[i].integer == false) {
            state.opt_cfg.map.to_ps[n_params] = i;
            state.opt_cfg.map.to_opt[i] = n_params;
            // initial_x[n_params] = state.info.params[i].default_value > 0 ? 1.0f : -1.0f;
            normalization_factors[n_params] = fabsf(state.info.params[i].default_value);
            n_params++;
        }
    }
    i32 n_samples = (i32)buf_len(state.files);
    state.opt = optimize(n_params, initial_x, n_samples);
    // since we use divided difference to get the derivative we scale all parameters before feeding them to the optimizer
    state.opt_cfg.normalization_factor = normalization_factors;
    state.opt_cfg.barriers[0] = 2.0f;
    state.opt_cfg.barriers[1] = 2.0f;
    state.opt_cfg.barriers[2] = 2.0f;
    state.opt_cfg.barriers[3] = 2.0f;
    state.opt_cfg.barriers[4] = 2.0f;
    state.opt_cfg.barriers[5] = 5.0f;
    state.opt_cfg.barriers[6] = 2.0f;
    state.opt_cfg.barriers[7] = 2.5f;
    // some parameters can blow up the calc
    buf_pushn(state.opt_cfg.bounds, n_params);
    for (isize i = 0; i < n_params; i++) {
        state.opt_cfg.bounds[i].low = -INFINITY;
        state.opt_cfg.bounds[i].high = INFINITY;
    }
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("StreamMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("StreamMod"), S("jack_comp_min"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("StreamMod"), S("jack_comp_max"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("JSMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("HSMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("CJMod"), S("prop_buffer"))] = (Bound) { 0.0f, 2.0f - 0.0001f };
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("CJMod"), S("total_prop_scaler"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("RunningManMod"), S("offhand_tap_prop_min"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("WideRangeRollMod"), S("max_mod"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("Globals"), S("MinaCalc.tech_pbm"))].low = 1.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("Globals"), S("MinaCalc.jack_pbm"))].low = 1.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("Globals"), S("MinaCalc.stream_pbm"))].low = 1.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("Globals"), S("MinaCalc.bad_newbie_skillsets_pbm"))].low = 1.0f;
#if DUMP_CONSTANTS == 0 // almost always
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("OHJ.h(78)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("OHJ.h(86)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(91, 2)"))].low = 0.1f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(109)"))].low = 0.1f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(163)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(163, 2)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(593)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(593)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(864)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(866)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(869)"))].low = 0.0f;
    state.opt_cfg.bounds[param_opt_index_by_name(&state.info, state.opt_cfg.map.to_opt, S("InlineConstants"), S("MinaCalc.cpp(871)"))].low = 0.0f;
#endif
    for (isize i = 0; i < n_params; i++) {
        state.opt_cfg.bounds[i].low /= normalization_factors[i];
        state.opt_cfg.bounds[i].high /= normalization_factors[i];
    }
}

static f32 SkillsetOverallBalance = 0.35f;
void optimizer_skulduggery(SimFileInfo *sfi, ParameterLossWork work, SkillsetRatings ssr)
{
    assert(state.target.msd_sd > 0.0f);
    f32 delta_real = ssr.E[sfi->target.skillset] - work.msd;
    f32 delta_overall = ssr.overall - work.msd;
    f32 misclass = expf(ssr.overall / ssr.E[sfi->target.skillset]) / 2.71828182846f;
    f32 magnify_large_targets = work.msd / state.target.msd_mean;
    f32 delta = misclass * magnify_large_targets * lerp(delta_real, delta_overall, SkillsetOverallBalance);
    buf_push(state.opt_evaluations, (OptimizationEvaluation) {
        .sample = work.sample,
        .param = state.opt_cfg.map.to_opt[work.param],
        .delta = delta,
        .barrier = state.opt_cfg.barriers[sfi->target.skillset]
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
    Buffer font = load_font_file("web/NotoSansCJKjp-Regular.otf");
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

#if !defined(EMSCRIPTEN)
    add_target_files();
    setup_optimizer();
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
                tooltip("CalcTestList: %s delta", SkillsetNames[i]);
                if (i == 0) {
                    igSameLine(0, 0); igTextUnformatted(" (", 0); igSameLine(0, 0);
                    igTextColored(msd_color(fabsf(state.target.min_delta) * 3.0f), "%02.2f", (f64)state.target.min_delta);
                    tooltip("CalcTestList: min delta");
                    igSameLine(0, 0); igTextUnformatted(", ", 0); igSameLine(0, 0);
                    igTextColored(msd_color(fabsf(state.target.max_delta) * 3.0f), "%02.2f", (f64)state.target.max_delta);
                    tooltip("CalcTestList: max delta"); ;
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
    b32 show_parameter_names = state.last_frame.show_parameter_names;

    f32 left_width = show_parameter_names ? 450.0f : 302.0f;
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
            // Arrow dropdown fake thing again
            igSameLine(0, 0);
            igPushStyleColorU32(ImGuiCol_HeaderHovered, 0);
            igPushStyleColorU32(ImGuiCol_HeaderActive, 0);
            show_parameter_names = igTreeNodeExStr("##parameter name toggle", ImGuiTreeNodeFlags_NoTreePushOnOpen);
            tooltip("show parameter names");
            igPopStyleColor(2);

            // The actual tabs
            if (igBeginTabItem("More Relevant", 0, ImGuiTabItemFlags_None)) {
                tooltip("These will basically always change the MSD of the active file's selected skillsets");
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if (active->effects.strong == 0 || (active->effects.strong[mp] & effect_mask) != 0) {
                                param_slider_widget(mp, show_parameter_names, &changed_param);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            }
            if (igBeginTabItem("Relevant", 0, ImGuiTabItemFlags_None)) {
                tooltip("More, plus some params that need more shoving");
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if (active->effects.weak == 0 || (active->effects.weak[mp] & effect_mask) != 0) {
                                param_slider_widget(mp, show_parameter_names, &changed_param);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            }
            if (igBeginTabItem("All", 0, ImGuiTabItemFlags_None)) {
                tooltip("Everything\nPretty useless unless you like finding out which knobs do nothing yourself");
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            param_slider_widget(mp, show_parameter_names, &changed_param);
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            }
            if (igBeginTabItem("Dead", 0, ImGuiTabItemFlags_None)) {
                tooltip("These don't do anything to the active file's selected skillsets");
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if (active->effects.weak && (active->effects.weak[mp] & effect_mask) == 0) {
                                param_slider_widget(mp, show_parameter_names, &changed_param);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            }
        }
        igEndTabBar();
    }
    igEnd();

    if (changed_param.type == ParamSlider_ValueChanged) {
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
                calculate_effects(sfi == active ? &state.high_prio_work : &state.low_prio_work, &state.info, sfi, state.generation);

                if (igIsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                    next_active = sfi;
                }

                // Unset fullscreen window on drag
                if (igIsWindowHovered(ImGuiHoveredFlags_RootWindow) && igIsMouseDragging(0, -1.f)) {
                    sfi->fullscreen_window = false;
                }

                // File difficulty + chartkey text
                igTextColored(msd_color(sfi->aa_rating), "%.2f", (f64)sfi->aa_rating);
                igSameLine(clamp_low(GetContentRegionAvailWidth() - 235.f, 100), 0);
                igText(sfi->chartkey.buf);

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
    if (state.loss_window) {
        if (!state.optimization_graph) {
            state.optimization_graph = &state.graphs[make_optimization_graph()];
        }

        static u32 last_generation = 0;
        if (last_generation != state.generation) {
            assert(!"Lazy");
            // memcpy(state.opt.x, state.ps.params + 1, state.opt.n_params * sizeof(f32));
            last_generation = state.generation;
        } else if (state.opt_pending_evals == 0) {
            state.generation++;
            last_generation = state.generation;

            OptimizationRequest req = opt_pump(&state.opt, state.opt_evaluations);
            assert(req.n_samples > 0);
            buf_clear(state.opt_evaluations);

            for (isize i = 0; i < state.opt.n_params; i++) {
                // Apply bounds. This is a kind of a hack so it lives out here and not in the optimizer :)
                state.opt.x[i] = clamp(state.opt_cfg.bounds[i].low, state.opt_cfg.bounds[i].high, state.opt.x[i]);
                // Copy x into the calc parameters
                state.ps.params[state.opt_cfg.map.to_ps[i]] = state.opt.x[i] * state.opt_cfg.normalization_factor[i];
            }

            i32 submitted = 0;
            for (isize i = 0; i < req.n_samples; i++) {
                i32 sample = state.opt.active.samples[i];
                i32 file_index = sample % buf_len(state.files);
                assert(file_index >= 0);
                SimFileInfo *sfi = &state.files[file_index];

                f32 goal = 0.93f;
                f32 msd = sfi->target.want_msd * (39.0f / 40.0f);

                ParameterLossWork plw_x = { .sample = sample, .goal = goal, .msd = msd };
                calculate_parameter_loss(&state.low_prio_work, sfi, plw_x, state.generation);
                submitted++;

                i32 params_submitted = 0;
                for (isize j = 0; j < state.opt.n_params; j++) {
                    i32 param = state.opt.active.parameters[j];
                    i32 p = state.opt_cfg.map.to_ps[param];
                    if (sfi->num_effects_computed == 0 || sfi->effects.weak[p]) {
                        calculate_parameter_loss(&state.low_prio_work, sfi, (ParameterLossWork) {
                            .sample = sample,
                            .param = p,
                            .value = (state.opt.x[param] + req.h) * state.opt_cfg.normalization_factor[param],
                            .goal = goal,
                            .msd = msd
                        }, state.generation);
                        params_submitted++;
                    }

                    if (params_submitted == req.n_parameters) {
                        break;
                    }
                }

                submitted += params_submitted;

                if (buf_len(state.high_prio_work) == 0 && ((sfi->num_effects_computed == 0) || ((state.generation - sfi->effects_generation) > 300))) {
                    sfi->num_effects_computed = 0;
                    calculate_effects(&state.high_prio_work, &state.info, sfi, state.generation);
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

        igSetNextWindowPos(V2(left_width, 0), ImGuiCond_Always, V2Zero);
        igSetNextWindowSize(V2(centre_width, ds.y), ImGuiWindowFlags_NoResize);
        if (igBegin("Loss", &state.loss_window, 0)) {
            num_open_windows++;
            f32 err_lim = 2.0f;
            f32 loss_lim = FLT_MIN;
            for (isize i = 0; i < NumGraphSamples; i++) {
                err_lim = max(err_lim, state.optimization_graph->ys[0][i]);
                loss_lim = max(loss_lim, state.optimization_graph->ys[1][i]);
            }
            ipSetNextPlotLimits(0, NumGraphSamples, 0, err_lim, ImGuiCond_Always);
            if (BeginPlotDefaultsFullWidth("##Average Error", "Iteration", "Average Error")) {
                skillset_line_plot(0, false, state.optimization_graph, state.optimization_graph->ys[0]);
                ipEndPlot();
            }
            ipSetNextPlotLimits(0, NumGraphSamples, 0, loss_lim, ImGuiCond_Always);
            if (BeginPlotDefaultsFullWidth("##Loss", "Iteration", "Loss")) {
                skillset_line_plot(0, false, state.optimization_graph, state.optimization_graph->ys[1]);
                ipEndPlot();
            }
        }

        igSliderFloat("H", &H, 1.0e-8f, 0.1f, "%g", 1.0f);
        igSliderFloat("StepSize", &StepSize, 1.0e-8f, 0.1f, "%g", 1.0f);
        igSliderFloat("MDecay", &MDecay, 0.666f, 1.0f - 1e-3f, "%g", 0.5f);
        igSliderFloat("VDecay", &VDecay, 0.666f, 1.0f - 1e-5f, "%g", 0.5f);
        igSliderInt("SampleBatchSize", &SampleBatchSize, 1, state.opt.n_samples, "%d");
        igSliderInt("ParameterBatchSize", &ParameterBatchSize, 1, state.opt.n_params, "%d");
        igSliderFloat("Stream Overrated Penalty", &state.opt_cfg.barriers[1], 2.0f, 8.0f, "%f", 1.0f);
        igSliderFloat("JS Overrated Penalty", &state.opt_cfg.barriers[2], 2.0f, 8.0f, "%f", 1.0f);
        igSliderFloat("HS Overrated Penalty", &state.opt_cfg.barriers[3], 2.0f, 8.0f, "%f", 1.0f);
        igSliderFloat("Stamina Overrated Penalty", &state.opt_cfg.barriers[4], 2.0f, 8.0f, "%f", 1.0f);
        igSliderFloat("Jackspeed Overrated Penalty", &state.opt_cfg.barriers[5], 2.0f, 8.0f, "%f", 1.0f);
        igSliderFloat("Chordjacks Overrated Penalty", &state.opt_cfg.barriers[6], 2.0f, 8.0f, "%f", 1.0f);
        igSliderFloat("Technical Overrated Penalty", &state.opt_cfg.barriers[7], 2.0f, 8.0f, "%f", 1.0f);
        igSliderFloat("Skillset/Overall Balance", &SkillsetOverallBalance, 0.0f, 1.0f, "%f", 1.0f);

        igEnd();
    }

    // Start up text window
    if (buf_len(state.files) == 0) {
        igSetNextWindowPos(V2(left_width + centre_width / 2.0f, ds.y / 2.f - 40.f), 0, V2(0.5f, 1.0f));
        igBegin("MinaButan", 0, ImGuiWindowFlags_NoDecoration);
        if (igButton("Add Mina's CalcTestList.xml", V2Zero)) {
            add_target_files();
        }
        igEnd();

        igSetNextWindowPos(V2(left_width + centre_width / 2.0f, ds.y / 2.f), 0, V2(0.5f, 0.5f));

        igBegin("Drop", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        igText("Drop files, song folders or packs here");
        igEnd();
#ifdef NO_SSE
        igSetNextWindowPos(V2(left_width + centre_width / 2.0f, ds.y / 2.f + 20.f), 0, V2(0.5f, 0.0f));
        igBegin("SSE", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        igTextColored((ImVec4) { 1, 0, 0, 1.0f}, "No SSE available");
        igText("Turn on SIMD support in about:flags if you want the calc to be exactly the same as in game");
        igTextColored((ImVec4) { 1, 1, 1, 0.5f}, "(Computer touchers: this is a numerical precision thing, not a speed thing)");
        igEnd();
#endif
    }

    // Debug window
    if (igIsKeyPressed('`', false)) {
        state.debug_window = !state.debug_window;
    }
    if (state.debug_window) {
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

        DUMP_CONSTANT_INFO;
    }

    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();

    state.last_frame.num_open_windows = num_open_windows;
    state.last_frame.show_parameter_names = show_parameter_names;
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
    (void)argc;
    (void)argv;

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
