#if !(defined(SOKOL_GLCORE33)||defined(SOKOL_GLES2)||defined(SOKOL_GLES3)||defined(SOKOL_D3D11)||defined(SOKOL_METAL)||defined(SOKOL_WGPU)||defined(SOKOL_DUMMY_BACKEND))
#define SOKOL_GLCORE33
#endif

#ifdef __clang__
// Disabling these warnings is for libraries only.
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
    String display_id;

    SkillsetRatings aa_rating;
    SkillsetRatings max_rating;

    NoteData *notes;
    i32 n_rows;
    EffectMasks effects;
    i32 *graphs;

    struct {
        f32 want_msd;
        f32 rate;
        i32 skillset;

        f32 last_user_set_weight;
        f32 weight;

        f32 got_msd;
        f32 delta;
    } target;

    SkillsetRatings default_ratings;
    u32 skillsets_generation;
    u32 num_effects_computed;
    u32 effects_generation;

    bool selected_skillsets[NumSkillsets];
    bool display_skillsets[NumSkillsets];

    DebugInfo debug_info;
    u32 debug_generation;

    bool open;
    bool stops;
    u64 frame_last_focused;
} SimFileInfo;

bool sfi_visible_in_right_pane(SimFileInfo *sfi)
{
    return sfi->target.weight != 0.0f;
}

static SimFileInfo null_sfi_ = {0};
static SimFileInfo *const null_sfi = &null_sfi_;

typedef struct TargetFile
{
    u64 id;
    String key;
    String author;
    String title;
    i32 difficulty;
    i32 skillset;
    f32 rate;
    f32 target;
    f32 weight;
    b32 loaded;
} TargetFile;

typedef struct CalcTestListLoader {
    b32 workin;
    Buffer xml;
    TargetFile *requested_files;
    f32 mean, unscaled_variance;
} CalcTestListLoader;

typedef struct DBFile DBFile;
typedef struct Search {
    usize id;
    String query;
    DBFile *results;
} Search;

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
    CalcWork *debug_graph_work;
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
    b32 reset_optimizer_flag;
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
            i32 *to_file;
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

    struct {
        f32 left_width;
        f32 right_width;
    } last_frame;

    Buffer *dropped_files;
    CalcTestListLoader loader;
    Search search;
} State;
static State state = {0};

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
    buf_printf(str, "#calc version: %d\n\n", state.info.version);
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

static i32 parse_checkpoint_version(SmParser *ctx)
{
    if (try_consume_string(ctx, S("#calc version:"))) {
        return (i32)parse_f32(ctx);
    }
    return 0;
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
        consume_whitespace_and_comments(ctx);
        i32 v = parse_checkpoint_version(ctx);
        if (v == 0) {
            printf("checkpoints error: checkpoints has no version string\n");
            return;
        }
        if (v != state.info.version) {
            printf("checkpoints error: seeminacalc has calc version %d, checkpoints is for %d\n", state.info.version, v);
            return;
        }

        SmTagValue s, t;
        while (try_advance_to_and_parse_checkpoint_tag(ctx, &s)) {
            b32 t_ok = try_advance_to_and_parse_checkpoint_tag(ctx, &t);
            if (t_ok == false) {
                break;
            }
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
    calculate_skillsets_in_background(&state.low_prio_work, state.generation);
}

static const ImVec2 V2Zero = {0};

static bool BeginPlotDefaults(const char* title_id, const char* x_label, const char* y_label)
{
    return ImPlot_BeginPlot(title_id, x_label, y_label, (ImVec2){igGetWindowWidth() / 2.0f - 8.0f, 0}, ImPlotFlags_NoLegend|ImPlotFlags_NoChild, ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock, 0, 0);
}
static bool BeginPlotOptimizer(const char* title_id)
{
    return ImPlot_BeginPlot(title_id, "", "Error", (ImVec2){-1, 0}, ImPlotFlags_YAxis2, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None, ImPlotAxisFlags_None, ImPlotAxisFlags_None, "Loss", 0);
}

static ImVec4 GetColormapColor(int index)
{
    ImVec4 result;
    ImPlot_GetColormapColor(&result, index, 3);
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

static void TextString(String s)
{
    igTextUnformatted(s.buf, s.buf + s.len);
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
#if 0
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
#endif
    return 0;
}

void load_target_files(CalcTestListLoader *loader, Buffer test_list_xml)
{
    buf_clear(loader->requested_files);
    XML xml = xml_begin(test_list_xml);
    while (xml_open(&xml, S("CalcTestList"))) {
        String skillset = xml_attr(&xml, S("Skillset"));
        i32 ss = string_to_i32(skillset);

        if (ss > 0 && ss < NumSkillsets) {
            while (xml_open(&xml, S("Chart"))) {
                String key = xml_attr(&xml, S("aKey"));
                String rate = xml_attr(&xml, S("bRate"));
                String target = xml_attr(&xml, S("cTarget"));

                if (xml.ok) {
                    u64 id = db_get(key);
                    buf_push(loader->requested_files, (TargetFile) {
                        .id = id,
                        .key = key,
                        .rate = string_to_f32(rate),
                        .target = string_to_f32(target),
                        .skillset = ss,
                        .weight = 1.0f,
                    });
                }

                xml_close(&xml, S("Chart"));
            }
        } else {
            printf("CalcTestList parse error: skillset: %.*s (should be 1-7)\n", (i32)skillset.len, skillset.buf);
        }

        xml_close(&xml, S("CalcTestList"));
    }

    if (buf_len(loader->requested_files) > 0) {
        loader->workin = true;
        loader->xml = test_list_xml;
    } else {
        free(test_list_xml.buf);
    }
}

b32 process_target_files(CalcTestListLoader *loader, DBResult results[])
{
    if (loader->workin == false || buf_len(results) == 0) {
        return false;
    }

    // Todo: move these off loader, they aren't tied to a single load
    f32 target_m = loader->mean;
    f32 target_s = loader->unscaled_variance;

    isize added_count = 0;
    isize req_index = 0;
    for (; req_index < buf_len(loader->requested_files); req_index++) {
        if (loader->requested_files[req_index].loaded == false) {
            break;
        }
    }

    for (isize result_index = 0; result_index < buf_len(results); result_index++, req_index++) {
        assert(req_index < buf_len(loader->requested_files));
        if (results[result_index].type == DBRequest_File) {
            DBResult *result = &results[result_index];
            DBFile *file = &result->file;
            TargetFile *req = &loader->requested_files[req_index];

            if (result->id != req->id) {
                assert_unreachable();
                goto give_up;
            }

            if (req->loaded) {
                assert_unreachable();
                goto give_up;
            }

            req->loaded = true;

            if (result->file.ok == false)  {
                continue;
            }

            push_allocator(scratch);
            String ck = file->chartkey;
            String title = file->title;
            String author = file->author;
            String diff = SmDifficultyStrings[file->difficulty];
            String id = {0};
            id.len = buf_printf(id.buf, "%.*s ", title.len, title.buf);
            if (req->rate != 1.0f) {
                id.len += buf_printf(id.buf, "%.1f ", (f64)req->rate);
            }
            id.len += buf_printf(id.buf, "(%.*s%.*s%.*s)",
                author.len, author.buf,
                author.len == 0 ? 0 : 2, ", ",
                diff.len, diff.buf);
            isize display_id_len = id.len;
            id.len += buf_printf(id.buf, "##%.*s.%d",
                ck.len, ck.buf,
                req->skillset);
            pop_allocator();

            for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                if (strings_are_equal(sfi->id, id)) {
                    goto skip;
                }
            }

            if (buf_len(state.files) == buf_cap(state.files)) {
                printf("please.. no more files");
                goto give_up;
            }

            id = copy_string(id);
            String display_id = (String) { id.buf, display_id_len };
            SimFileInfo *sfi = buf_push(state.files, (SimFileInfo) {
                .title = copy_string(title),
                .diff = diff,
                .chartkey = copy_string(ck),
                .id = id,
                .display_id = display_id,
                .target.want_msd = req->target,
                .target.rate = req->rate,
                .target.skillset = req->skillset,
                .target.weight = req->weight,
                .target.last_user_set_weight = req->weight,
                .notes = file->note_data,
                .n_rows = file->n_rows,
                .selected_skillsets[0] = true,
            });

            buf_push(sfi->graphs, make_skillsets_graph());
            buf_reserve(sfi->effects.weak, state.info.num_params);
            buf_reserve(sfi->effects.strong, state.info.num_params);

            calculate_skillsets(&state.high_prio_work, sfi, true, state.generation);

            f32 old_m = target_m;
            f32 old_s = target_s;
            target_m = old_m + (req->target - old_m) / (f32)(buf_len(state.files) + 1);
            target_s = old_s + (req->target - old_m)*(req->target - target_m);

            printf("Added %s %s %02.2fx: %02.2f\n", SkillsetNames[req->skillset], title.buf, (f64)req->rate, (f64)req->target);

            added_count++;
skip:;
        }
    }

    b32 all_done = true;
    for (isize i = 0; i < buf_len(loader->requested_files); i++) {
        if (loader->requested_files[i].loaded == false) {
            all_done = false;
            break;
        }
    }

    if (all_done) {
give_up:
        loader->workin = false;
        free(loader->xml.buf);
        loader->xml = (Buffer) {0};
        buf_clear(loader->requested_files);
    }

    loader->mean = target_m;
    loader->unscaled_variance = target_s;

    f32 target_var = target_s / (f32)(buf_len(state.files) - 1);
    state.target.msd_mean = target_m;
    state.target.msd_sd = sqrtf(target_var);

    return added_count > 0;
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
    if (param_idx == 0) {
        // Stupid hack to not show the slider for rate, but it can still be graphed
        if (igCheckbox("##graph", &state.parameter_graphs_enabled[param_idx])) {
            out->type = ParamSlider_GraphToggled;
            out->param = param_idx;
        } tooltip("graph this parameter");
        return;
    }
    assert(out);
    i32 type = ParamSlider_Nothing;
    f32 value = 0.0f;
    i32 mp = param_idx;
    igPushID_Int(mp);
    igBeginDisabled(!state.info.params[mp].optimizable);
    if (igCheckbox("##opt", &state.opt_cfg.enabled[mp])) {
        assert(state.info.params[mp].optimizable);
        type = ParamSlider_OptimizingToggled;
    } tooltip("optimize this parameter");
    igEndDisabled();
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
        igSetNextItemWidth(-FLT_MIN);
        if (igSliderInt(slider_id, &value_int, low, high, "%d", ImGuiSliderFlags_None)) {
            state.ps.params[mp] = (f32)value_int;
            type = ParamSlider_ValueChanged;
            value = (f32)value_int;
        }
    } else {
        igSetNextItemWidth(-FLT_MIN);
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
    if (igTreeNodeEx_Str(mod->name, ImGuiTreeNodeFlags_DefaultOpen)) {
        i32 count = 0;
        for (i32 i = 0; i < mod->num_params; i++) {
            i32 mp = mod->index + i;
            if (effects == 0 || (effects[mp] & effect_mask) != 0 || state.opt_cfg.enabled[mp]) {
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
                if (effects == 0 || (effects[mp] & effect_mask) != 0 || state.opt_cfg.enabled[mp]) {
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
        ImPlot_PushStyleVar_Float(ImPlotStyleVar_LineWeight, ImPlot_GetStyle()->LineWeight * 2.0f);
    }

    ImPlot_PushStyleColor_U32(ImPlotCol_Line, state.skillset_colors[ss]);
    ImPlot_PlotLine_FloatPtrFloatPtr(SkillsetNames[ss], fng->xs, ys, fng->len, 0, sizeof(float));
    ImPlot_PopStyleColor(1);

    if (highlight) {
        ImPlot_PopStyleVar(1);
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
static f32 ExpScale = 0.0f;
static f32 Misclass = 0.0f;
#if 0
static f32 UnderratedDeadZone = 0.5f;
#endif
void setup_optimizer(void)
{
    f32 *normalization_factors = 0;
    f32 *initial_x = 0;
    push_allocator(scratch);
    buf_pushn(initial_x, state.ps.num_params);
    pop_allocator();

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
        state.opt_cfg.enabled[i] = false && state.info.params[i].optimizable && !state.info.params[i].constant;
        // since we use finite differences to get the derivative we scale all parameters before feeding them to the optimizer
        normalization_factors[i] = clamp_low(1.0f, absolute_value(state.info.params[i].default_value));
    }
    i32 n_params = pack_opt_parameters(&state.info, state.ps.params, normalization_factors, state.opt_cfg.enabled, initial_x, state.opt_cfg.map.to_ps, state.opt_cfg.map.to_opt);
    i32 n_samples = (i32)buf_len(state.files);
    optimize(&state.opt, n_params, initial_x, n_samples);
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
#if DUMP_CONSTANTS == 0
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
    // Loss fine-tuning. Disabled in the UI and set to the identity function
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
    if (ExpScale) target = lerp(target, expf(target), ExpScale);
    f32 skillset = (ssr.E[ss] - mean) / state.target.msd_sd;
    if (ExpScale) skillset = lerp(skillset, expf(skillset), ExpScale);
    f32 overall = (ssr.overall - mean) / state.target.msd_sd;
    if (ExpScale) overall = lerp(overall, expf(overall), ExpScale);
    f32 delta_skillset = skillset - target;
    f32 delta_overall = overall - target;
    f32 ssr_largest = 0;
    for (isize i = 1; i < NumSkillsets; i++) {
        ssr_largest = max(ssr_largest, ssr.E[i]);
    }
    f32 misclass = 1.0f;
    if (Misclass) misclass = expf(Misclass * (ssr_largest / ssr.E[ss] - 1.0f));
    f32 delta = sfi->target.weight * misclass * lerp(delta_skillset, delta_overall, SkillsetOverallBalance);
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
    setup_allocators();

    push_allocator(scratch);
    Buffer font = read_file("web/NotoSansCJKjp-Regular.otf");
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
    ImPlot_CreateContext();
    state = (State) {
        .pass_action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f }
            }
        }
    };

    ImVec4 bg = *igGetStyleColorVec4(ImGuiCol_WindowBg);
    bg.w = 1.0f;
    igPushStyleColor_Vec4(ImGuiCol_WindowBg, bg);
    igPushStyleVar_Float(ImGuiStyleVar_ScrollbarSize, 4.f);
    igPushStyleVar_Float(ImGuiStyleVar_WindowRounding, 1.0f);

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
        img_desc.data.subimage[0][0].ptr = font_pixels;
        img_desc.data.subimage[0][0].size = font_width * font_height * sizeof(uint32_t);
        img_desc.label = "sokol-imgui-font";
        _simgui.img = sg_make_image(&img_desc);
        io->Fonts->TexID = (ImTextureID)(uintptr_t) _simgui.img.id;
    }

    state.generation = 1;
    state.info = calc_info();
    state.calc = calc_init(&state.info);
    state.ps = copy_param_set(&state.info.defaults);
    buf_pushn(state.parameter_graphs_enabled, state.info.num_params);
    buf_reserve(state.graphs, 1024);

    // todo: use handles instead
    buf_reserve(state.files, 1024);

    state.last_frame.right_width = 438.0f;
    state.last_frame.left_width = 322.0f;

    calc_sem = sem_create();
    high_prio_work_queue.lock = lock_create();
    low_prio_work_queue.lock = lock_create();

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

    if (db_path) {
        Buffer db = read_file_malloc(db_path);
        if (db.buf) {
            buf_push(state.dropped_files, db);
        }
    }

    if (test_list_path) {
        Buffer list = read_file_malloc(test_list_path);
        if (list.buf) {
            buf_push(state.dropped_files, list);
        }
    }

    setup_optimizer();
    checkpoint_default();
    checkpoint_latest();
    load_checkpoints_from_disk();
}

void frame(void)
{
    i32 width = sapp_width();
    i32 height = sapp_height();
    f64 delta_time = stm_sec(stm_laptime(&state.last_time));
    simgui_new_frame(width, height, delta_time);

    reset_scratch();
    finish_work();

    for (isize i = buf_len(state.dropped_files) - 1; i >= 0; i--) {
        Buffer *b = &state.dropped_files[i];

        if (buffer_begins_with(b, S("SQLite format 3\0"))) {
            // Assume sqlite database
            db_use(*b);
            buf_remove_unsorted(state.dropped_files, b);
        } else if (buffer_first_nonwhitespace_char(b) == '<') {
            // Assume CalcTestList.xml
            if (db_ready() && buf_len(state.loader.requested_files) == 0) {
                load_target_files(&state.loader, *b);
                buf_remove_unsorted(state.dropped_files, b);
            }
        } else if ((buffer_first_nonwhitespace_char(b) == '/') || (buffer_first_nonwhitespace_char(b) == '#')) {
            // Assume .sm
            free(b->buf);
        } else {
            free(b->buf);
            buf_remove_unsorted(state.dropped_files, b);
            printf("Bad dropped file\n");
        }
    }

    DBResultsByType db_results = db_pump();
    state.reset_optimizer_flag = process_target_files(&state.loader, db_results.of[DBRequest_File]);

    // Search stuff
    buf_reserve(state.search.results, 1024);
    for (isize i = 0; i < buf_len(db_results.of[DBRequest_Search]); i++) {
        DBResult *r = db_results.of[DBRequest_Search] + i;
        if (r->id == state.search.id) {
            buf_push(state.search.results, r->file);
        }
    }

    // Optimization stuff
    static char const *optimizer_error_message = 0;
    {
        if (!state.optimization_graph) {
            state.optimization_graph = &state.graphs[make_optimization_graph()];
        }

        static u32 last_generation = 1;
        if (last_generation != state.generation || state.reset_optimizer_flag) {
            // Some parameter has been messed with, or turned off
            // This invalidates the optimizer state either way, so just recreate the whole thing
            buf_clear(state.opt_evaluations);

            f32 *x = 0;
            push_allocator(scratch);
            buf_pushn(x, state.ps.num_params);
            pop_allocator();

            buf_clear(state.opt_cfg.map.to_file);
            for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                if (sfi->target.weight > 0.0f) {
                    buf_push(state.opt_cfg.map.to_file, (i32)buf_index_of(state.files, sfi));
                    if ((state.generation - sfi->effects_generation) > 1024) {
                        sfi->num_effects_computed = 0;
                        calculate_effects(&state.high_prio_work, &state.info, sfi, true, state.generation);
                    }
                }
            }

            i32 n_params = pack_opt_parameters(&state.info, state.ps.params, state.opt_cfg.normalization_factors, state.opt_cfg.enabled, x, state.opt_cfg.map.to_ps, state.opt_cfg.map.to_opt);
            i32 iter = state.opt.iter - 1;
            optimize(&state.opt, n_params, x, (i32)buf_len(state.opt_cfg.map.to_file));
            state.opt.iter = iter;

            last_generation = state.generation;
            state.opt_pending_evals = 0;

            state.reset_optimizer_flag = false;
        }

        if (state.optimizing) {
            if (state.opt.n_params == 0) {
                optimizer_error_message = "No parameters are enabled for optimizing.";
                state.optimizing = false;
            } else if (state.opt.n_samples == 0) {
                optimizer_error_message = "No files to optimize.";
                state.optimizing = false;
            } else if (state.opt_pending_evals == 0) {
                state.generation++;
                last_generation = state.generation;

                OptimizationRequest req = opt_pump(&state.opt, state.opt_evaluations);
                assert(req.n_samples > 0);
                buf_clear(state.opt_evaluations);

                for (isize i = 0; i < state.opt.n_params; i++) {
                    if (isnan(state.opt.x[i])) {
                        state.optimizing = false;
                        optimizer_error_message = "Cannot optimize due to NaNs. Reset to a good state.";
                        break;
                    }
                }

                // Do this even if we have NaNs so that they propagate to the UI.
                for (isize i = 0; i < state.opt.n_params; i++) {
                    isize p = state.opt_cfg.map.to_ps[i];
                    // Apply bounds. This is a hack so it lives out here and not in the optimizer :)
                    state.opt.x[i] = clamp(state.opt_cfg.bounds[p].low, state.opt_cfg.bounds[p].high, state.opt.x[i]);
                    // Copy x into the calc parameters
                    state.ps.params[p] = state.opt.x[i] * state.opt_cfg.normalization_factors[p];
                }

                if (state.optimizing) {
                    if (!optimizer_error_message) {
                        checkpoint_latest();
                    }
                    optimizer_error_message = 0;

                    i32 submitted = 0;
                    for (isize i = 0; i < req.n_samples; i++) {
                        i32 sample = state.opt.active.samples[i];
                        assert((u32)sample < buf_len(state.opt_cfg.map.to_file));
                        i32 file_index = state.opt_cfg.map.to_file[sample];
                        assert((u32)file_index < buf_len(state.files));
                        SimFileInfo *sfi = &state.files[file_index];
                        assert(sfi->target.weight > 0.0f);

                        i32 ss = sfi->target.skillset;
                        f32 goal = state.opt_cfg.goals[ss];
                        f32 msd = (sfi->target.want_msd + state.opt_cfg.bias[ss].add) * state.opt_cfg.bias[ss].mul;

                        i32 params_submitted = 0;
                        for (isize j = 0; j < state.opt.n_params; j++) {
                            i32 param = state.opt.active.parameters[j];
                            i32 p = state.opt_cfg.map.to_ps[param];
                            if (sfi->num_effects_computed == 0 || sfi->effects.weak[p]) {
                                calculate_parameter_loss(&state.low_prio_work, sfi, (ParameterLossWork) {
                                    .sample = sample,
                                    .param = p,
                                    .value = (state.opt.x[param] * state.opt_cfg.normalization_factors[p]) + req.h,
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
                    }

                    state.opt_pending_evals = submitted;

                    if (rngf() < 0.125f) {
                        calculate_skillsets(&state.high_prio_work, &state.files[state.opt.iter % buf_len(state.files)], false, state.generation);
                    }

                    isize x = state.opt.iter % NumGraphSamples;
                    state.optimization_graph->ys[0][x] = absolute_value(state.target.average_delta.E[0]);
                    state.optimization_graph->ys[1][x] = state.target.max_delta;
                    state.optimization_graph->ys[2][x] = state.opt.loss;
                }
            }
        }
    }

    bool ss_highlight[NumSkillsets] = {0};
    SimFileInfo *next_active = null_sfi;

    ImGuiIO *io = igGetIO();
    ImVec2 ds = io->DisplaySize;

    SimFileInfo *active = state.active;

    f32 right_width = state.last_frame.right_width;
    f32 left_width = state.last_frame.left_width;

    ParamSliderChange changed_param = {0};
    igSetNextWindowSizeConstraints(V2(0, -1), V2(FLT_MAX, -1), 0, 0);
    igSetNextWindowPos(V2(-1.0f, 0), ImGuiCond_Always, V2Zero);
    igSetNextWindowSize(V2(left_width + 1.0f, ds.y + 1.0f), ImGuiCond_Always);
    if (igBegin("Mod Parameters", 0, ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar)) {
        left_width = igGetWindowWidth() - 1.0f;
        u32 effect_mask = 0;
        isize clear_selections_to = -1;
        igPushID_Str(active->id.buf);

        // Skillset filters
        f32 selectable_width_factor = 4.2f;
        for (isize i = 0; i < NumSkillsets; i++) {
            igPushStyleColor_U32(ImGuiCol_Header, state.skillset_colors_selectable[i]);
            igPushStyleColor_U32(ImGuiCol_HeaderHovered, state.skillset_colors[i]);
            igSelectable_BoolPtr(SkillsetNames[i], &active->selected_skillsets[i], 0, V2((igGetWindowWidth() - 12*4) / selectable_width_factor, 0));
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

    // Main, center window
    f32 centre_width = ds.x - left_width - right_width;
    igSetNextWindowSizeConstraints(V2(0, -1), V2(ds.x - left_width, -1), 0, 0);
    igSetNextWindowPos(V2(left_width + 2.0f, 2.0f), ImGuiCond_Always, V2Zero);
    igSetNextWindowSize(V2(centre_width - 4.0f, ds.y - 52.0f), ImGuiCond_Always);
    if (igBegin("SeeMinaCalc", 0, ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove)) {
        centre_width = igGetWindowWidth() + 4.0f;
        if (igBeginTabBar("main", ImGuiTabBarFlags_None)) {
            if (igBeginTabItem("Halp", 0,0)) {
                igTextWrapped("There are two ways to get files into this thing: drag and drop simfiles, or drag and drop a cache.db and use the search tab. You can also drag on a CalcTestList.xml once cache.db is loaded.\n\n"
                    "Alternatively for the native binary only:\n"
                    "Place the .exe next to a cache.db and CalcTestList.xml, or run on the command line like:\n\n"
                    "    seeminacalc db=/etterna/Cache/cache.db list=/path/to/CalcTestList.xml\n\n"
                    "For any given file, most parameters (on the left) will do nothing. So, by default, parameters that have no effect on the rating of the most recently selected file are filtered out.\n\n"
                    "You can set the ratings you want files to have for particular skillsets and the optimizer will fiddle with numerical values in the calc and try to make it happen.\n\n"
                    "This problem is uh \"ill-conditioned\" so the optimizer does not having a stopping criterion. Have fun");

#ifdef NO_SSE
                igTextColored((ImVec4) { 1, 0, 0, 1.0f}, "No SSE available");
                igTextWrapped("If you are on an x64 machine, turn on SIMD support in about:flags if you want the calc to be the same as in game (this is a numerical precision thing, not a speed thing)");
#endif
                igEndTabItem();
            }

            if (igBeginTabItem("Files", 0,0)) {
                if (igBeginTable("FileTable", 8, ImGuiTableFlags_SizingStretchProp, V2Zero, 0)) {
                    igTableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 20.0f, 0);
                    igTableSetupColumn("File", 0, 1.0f, 0);
                    igTableSetupColumn("Skillset", ImGuiTableColumnFlags_WidthFixed, 105.0f, 0);
                    igTableSetupColumn("Rate", ImGuiTableColumnFlags_WidthFixed, 56.0f, 0);
                    igTableSetupColumn("WantMSD", ImGuiTableColumnFlags_WidthFixed, 40.0f, 0);
                    igTableSetupColumn("Meight", ImGuiTableColumnFlags_WidthFixed, 40.0f, 0);
                    igTableSetupColumn("Message", ImGuiTableColumnFlags_WidthFixed, 15.0f, 0);
                    igTableSetupColumn("MSD", ImGuiTableColumnFlags_WidthFixed, -FLT_MIN, 0);
                    ImGuiListClipper clip = {0};
                    ImGuiListClipper_Begin(&clip, buf_len(state.files), 0.0f);
                    while (ImGuiListClipper_Step(&clip)) {
                        for (i32 sfi_index = clip.DisplayStart; sfi_index < clip.DisplayEnd; sfi_index++) {
                            SimFileInfo *sfi = state.files + sfi_index;
                            f32 weight_before_user_interaction = sfi->target.weight;
                            calculate_skillsets(&state.low_prio_work, sfi, false, state.generation);
                            if (sfi == active) {
                                igPushStyleColor_Vec4(ImGuiCol_Header, msd_color(sfi->aa_rating.overall));
                            }
                            igPushID_Int(sfi_index);
                            igTableNextRow(0, 0);

                            igTableSetColumnIndex(0);
                            igBeginDisabled(sfi->target.last_user_set_weight == 0.0f);
                            b8 enabled = (sfi->target.weight != 0.0f);
                            if (igCheckbox("##enabled", &enabled)) {
                                sfi->target.weight = enabled ? sfi->target.last_user_set_weight : 0.0f;
                            }
                            tooltip("optimize this file");
                            igEndDisabled();

                            igTableSetColumnIndex(1);
                            if (igSelectable_Bool(sfi->id.buf, sfi->open, ImGuiSelectableFlags_None, V2Zero)) {
                                if (sfi->open) {
                                    sfi->open = false;
                                } else {
                                    sfi->open = true;
                                    next_active = sfi;
                                }
                            }

                            igTableSetColumnIndex(2);
                            igPushItemWidth(-FLT_MIN);
                            if (igBeginCombo("##Skillset", SkillsetNames[sfi->target.skillset], 0)) {
                                for (isize ss = 1; ss < NumSkillsets; ss++) {
                                    b32 is_selected = (ss == sfi->target.skillset);
                                    igSelectable_Bool(SkillsetNames[ss], is_selected, 0, V2Zero);
                                    if (is_selected) {
                                        igSetItemDefaultFocus();
                                    }
                                }
                                igEndCombo();
                            }
                            igSameLine(0,4);

                            igTableSetColumnIndex(3);
                            igPushItemWidth(-FLT_MIN);
                            static char const *rates[] = { "0.7", "0.8", "0.9", "1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0" };
                            static float rates_f[] = { 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f };
                            for (isize r_find_selected = 0; r_find_selected < array_length(rates); r_find_selected++) {
                                if ((absolute_value(rates_f[r_find_selected] - sfi->target.rate) < 0.05f)
                                 && igBeginCombo("##Rate", rates[r_find_selected], 0)) {
                                    for (isize r = 0; r < array_length(rates); r++) {
                                        b32 is_selected = (absolute_value(rates_f[r] - sfi->target.rate) < 0.05f);
                                        igSelectable_Bool(rates[r], is_selected, 0, V2Zero);
                                        if (is_selected) {
                                            igSetItemDefaultFocus();
                                        }
                                    }
                                    igEndCombo();
                                }
                            }

                            igTableSetColumnIndex(4);
                            igPushItemWidth(-FLT_MIN);
                            igDragFloat("##target", &sfi->target.want_msd, 0.1f, 0.0f, 40.0f, "%02.2f", ImGuiSliderFlags_AlwaysClamp);
                            tooltip("target msd for skillset");

                            igTableSetColumnIndex(5);
                            igPushItemWidth(-FLT_MIN);
                            if (igDragFloat("##weight", &sfi->target.weight, 0.1f, 0.0f, 20.0f, "%02.2f", ImGuiSliderFlags_AlwaysClamp)) {
                                sfi->target.last_user_set_weight = sfi->target.weight;
                            }
                            tooltip("weight");

                            igTableSetColumnIndex(6);
                            if (absolute_value(sfi->target.delta) > 5.0f) {
                                igTextColored((ImVec4) { 0.85f, 0.85f, 0.0f, 0.95f }, "!");
                                tooltip("calc is off by %02.02f, are these right for this file?", (f64)sfi->target.delta);
                                igSameLine(0,4);
                            }
                            for (isize i = 1; i < NumSkillsets; i++) {
                                if (sfi->aa_rating.E[i] * 0.9f > sfi->aa_rating.E[sfi->target.skillset]) {
                                    if (absolute_value(sfi->target.delta) > 5.0f) {
                                        igTextColored((ImVec4) { 0.85f, 0.85f, 0.0f, 0.95f }, "?");
                                        tooltip("calc doesn't think this file is %s. is it?", SkillsetNames[sfi->target.skillset]);
                                        break;
                                    }
                                }
                            }

                            igTableSetColumnIndex(7);
                            for (isize ss = 0; ss < NumSkillsets; ss++) {
                                igTextColored(msd_color(sfi->aa_rating.E[ss]), "%s%02.2f", sfi->aa_rating.E[ss] < 10.f ? "0" : "", (f64)sfi->aa_rating.E[ss]);
                                tooltip(SkillsetNames[ss]);
                                igSameLine(0, 4);
                            }

                            if (sfi == active) {
                                igPopStyleColor(1);
                            }
                            igPopID();

                            if (sfi->target.weight != weight_before_user_interaction) {
                                state.generation++;
                            }
                        }
                    }
                    igEndTable();
                }
                igEndTabItem();
            }

            if (igBeginTabItem("Search", 0,0)) {
                if (db_ready()) {
                    static char q[512] = "";
                    static bool have_query = false;
                    bool have_new_query = false;
                    if (igInputText("##Search", q, 512, 0,0,0)) {
                        have_query = (q[0] != 0);
                        if (have_query) {
                            state.search.id = db_search((String) { q, strlen(q) });
                            have_new_query = true;
                        }
                    }
                    if (have_query) {
                        igSameLine(0, 4);
                        igText("%lld matches", buf_len(state.search.results));
                    }
                    if (igBeginTable("Search Results", 7, ImGuiTableFlags_Borders|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollX, V2Zero, 0)) {
                        igTableSetupColumn("Diff", ImGuiTableColumnFlags_WidthFixed, 65.0f, 0);
                        igTableSetupColumn("Author", 0, 2, 0);
                        igTableSetupColumn("Title", 0, 5, 0);
                        igTableSetupColumn("Subtitle", 0, 3, 0);
                        igTableSetupColumn("Artist", 0, 2, 0);
                        igTableSetupColumn("MSD", ImGuiTableColumnFlags_WidthFixed, 40.0f, 0);
                        igTableSetupColumn("Skillset", ImGuiTableColumnFlags_WidthFixed, 75.0f, 0);
                        igTableHeadersRow();
                        ImGuiListClipper clip = {0};
                        ImGuiListClipper_Begin(&clip, buf_len(state.search.results), 0.0f);
                        while (ImGuiListClipper_Step(&clip)) {
                            for (isize i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
                                DBFile *f = &state.search.results[i];
                                dbfile_set_skillset_and_rating_from_all_msds(f);
                                igTableNextRow(0, 0);
                                igTableSetColumnIndex(0);
                                TextString(SmDifficultyStrings[f->difficulty]);
                                igTableSetColumnIndex(1);
                                TextString(f->author);
                                igTableSetColumnIndex(2);
                                TextString(f->title);
                                igTableSetColumnIndex(3);
                                TextString(f->subtitle);
                                igTableSetColumnIndex(4);
                                TextString(f->artist);
                                igTableSetColumnIndex(5);
                                igTextColored(msd_color(f->rating), "%02.2f", (f64)f->rating);
                                igTableSetColumnIndex(6);
                                igText("%s", SkillsetNames[f->skillset]);
                            }
                        }

                        igEndTable();
                    }
                    if (have_new_query) {
                        // Delaying clearing the old search's results one frame looks a bit nicer
                        buf_clear(state.search.results);
                    }
                } else {
                    igTextWrapped("No cache.db :(");
                }
                igEndTabItem();
            }

            if (igBeginTabItem("Graphs", 0,0)) {
                if (buf_len(state.files) == 0) {
                    igTextWrapped("No files to graph :(");
                } else if (active == null_sfi) {
                    next_active = state.files;
                    next_active->open = true;
                } else {
                    SimFileInfo *sfi = active;

                    // File difficulty + chartkey text
                    igTextColored(msd_color(sfi->aa_rating.overall), "%02.2f", (f64)sfi->aa_rating.overall);
                    igSameLine(0, 8);
                    igSelectable_Bool(sfi->id.buf, false, ImGuiSelectableFlags_Disabled, V2Zero);
                    igSameLine(clamp_low(GetContentRegionAvailWidth() - 275.f, 100), 0);
                    igText(sfi->chartkey.buf);

                    // Plots. Weird rendering order: first 0, then backwards from the end. This keeps the main graph at the top and the rest most-to-least recent.
                    FnGraph *fng = &state.graphs[sfi->graphs[0]];
                    ImPlot_SetNextPlotLimits((f64)WifeXs[0] * 100, (f64)WifeXs[Wife965Index + 1] * 100, (f64)fng->min - 1.0, (f64)fng->max + 2.0, ImGuiCond_Always);
                    if (BeginPlotDefaults("Rating", "Wife%", "SSR")) {
                        calculate_file_graph(&state.high_prio_work, sfi, fng, state.generation);
                        for (i32 ss = 0; ss < NumSkillsets; ss++) {
                            skillset_line_plot(ss, ss_highlight[ss], fng, fng->ys[ss]);
                        }
                        ImPlot_EndPlot();
                    }
                    igSameLine(0, 4);
                    ImPlot_SetNextPlotLimits((f64)WifeXs[0] * 100, (f64)WifeXs[Wife965Index + 1] * 100, (f64)fng->relative_min - 0.05, 1.05, ImGuiCond_Always);
                    if (BeginPlotDefaults("Relative Rating", "Wife%", 0)) {
                        for (i32 ss = 1; ss < NumSkillsets; ss++) {
                            skillset_line_plot(ss, ss_highlight[ss], fng, fng->relative_ys[ss]);
                        }
                        ImPlot_EndPlot();
                    }

                    for (isize fungi = buf_len(sfi->graphs) - 1; fungi >= 1; fungi--) {
                        fng = &state.graphs[sfi->graphs[fungi]];
                        if (fng->param == changed_param.param && (changed_param.type == ParamSlider_LowerBoundChanged || changed_param.type == ParamSlider_UpperBoundChanged)) {
                            calculate_parameter_graph_force(&state.high_prio_work, sfi, fng, state.generation);
                        }

                        i32 mp = fng->param;
                        ParamInfo *p = &state.info.params[mp];
                        u8 full_name[64] = {0};
                        snprintf(full_name, sizeof(full_name), "%s.%s", state.info.mods[p->mod].name, p->name);
                        igPushID_Str(full_name);
                        ImPlot_SetNextPlotLimits((f64)state.ps.min[mp], (f64)state.ps.max[mp], (f64)fng->min - 1.0, (f64)fng->max + 2.0, ImGuiCond_Always);
                        if (BeginPlotDefaults("##AA", full_name, "AA Rating")) {
                            calculate_parameter_graph(&state.high_prio_work, sfi, fng, state.generation);
                            for (i32 ss = 0; ss < NumSkillsets; ss++) {
                                skillset_line_plot(ss, ss_highlight[ss], fng, fng->ys[ss]);
                            }
                            ImPlot_EndPlot();
                        }
                        igSameLine(0, 4);
                        ImPlot_SetNextPlotLimits((f64)state.ps.min[mp], (f64)state.ps.max[mp], (f64)fng->relative_min - 0.05, (f64)1.05, ImGuiCond_Always);
                        if (BeginPlotDefaults("##Relative AA", full_name, 0)) {
                            for (i32 ss = 1; ss < NumSkillsets; ss++) {
                                skillset_line_plot(ss, ss_highlight[ss], fng, fng->relative_ys[ss]);
                            }
                            ImPlot_EndPlot();
                        }
                        igPopID();
                    }
                }

                igEndTabItem();
            }

            if (igBeginTabItem("Song Preview", 0,0)) {
                if (buf_len(state.files) == 0) {
                    igTextWrapped("No files to preview :(");
                } else if (active == null_sfi) {
                    next_active = state.files;
                    next_active->open = true;
                } else {
                    static float x = 100, y_scale = 1000;
                    igSliderFloat("x", &x, 0, 200, "%f", 0);
                    igSliderFloat("y", &y_scale, 1.0f, 2000, "%f", 0);
                    ImTextureID my_tex_id = io->Fonts->TexID;
                    float my_tex_w = 50;
                    float my_tex_h = 80;
                    ImVec2 uv_min = V2(0.0f, 0.0f);
                    ImVec2 uv_max = V2(50.f / (float)io->Fonts->TexWidth, 80.0f / (float)io->Fonts->TexHeight);
                    ImVec4 tint_col = (ImVec4) {1.0f, 1.0f, 1.0f, 1.0f };
                    ImVec4 border_col = (ImVec4) {0};

                    if (ALWAYS(active->notes)) {
                        calculate_debug_graphs(&state.debug_graph_work, active, state.generation);

                        if (active->debug_generation == state.generation) {
                            DebugInfo *d = &active->debug_info;
                            isize n_rows = note_data_row_count(active->notes);
                            NoteInfo const *rows = note_data_rows(active->notes);
                            f32 last_row_time = rows[n_rows - 1].rowTime / active->target.rate;
                            ImVec2 pos = {0};
                            igSetNextWindowSize(V2(-1, last_row_time * y_scale), ImGuiCond_Always);
                            if (igBeginChild_Str("##Preview", V2Zero, false, 0)) {
                                igGetCursorPos(&pos);
                                f32 y_scale_rate = y_scale / active->target.rate;
                                for (isize i = 0; i < n_rows; i++) {
                                    if (rows[i].notes & 1) {
                                        igSetCursorPos(V2(1.0f * x, rows[i].rowTime * y_scale_rate - my_tex_h/2));
                                        igImage(my_tex_id, V2(my_tex_w, my_tex_h), uv_min, uv_max, tint_col, border_col);
                                    }
                                    if (rows[i].notes & 2) {
                                        igSetCursorPos(V2(2.0f * x, rows[i].rowTime * y_scale_rate - my_tex_h/2));
                                        igImage(my_tex_id, V2(my_tex_w, my_tex_h), uv_min, uv_max, tint_col, border_col);
                                    }
                                    if (rows[i].notes & 4) {
                                        igSetCursorPos(V2(3.0f * x, rows[i].rowTime * y_scale_rate - my_tex_h/2));
                                        igImage(my_tex_id, V2(my_tex_w, my_tex_h), uv_min, uv_max, tint_col, border_col);
                                    }
                                    if (rows[i].notes & 8) {
                                        igSetCursorPos(V2(4.0f * x, rows[i].rowTime * y_scale_rate - my_tex_h/2));
                                        igImage(my_tex_id, V2(my_tex_w, my_tex_h), uv_min, uv_max, tint_col, border_col);
                                    }
                                }
                                f32 scroll_y = igGetScrollY();
                                f32 scroll_y_time_lo = (scroll_y / y_scale) - 1.0f;
                                f32 scroll_y_time_hi = clamp_high(last_row_time + 1.0f, ((scroll_y + igGetWindowHeight()) / y_scale) + 1.0f);
                                isize scroll_y_interval_index_lo = (isize)(scroll_y_time_lo * 2.0f);
                                isize scroll_y_interval_index_hi = (isize)(scroll_y_time_hi * 2.0f);
                                isize interval_offset = clamps(0, d->n_intervals, scroll_y_interval_index_lo);
                                isize n_intervals = clamps(0, d->n_intervals - interval_offset, scroll_y_interval_index_hi - scroll_y_interval_index_lo);
                                assert(interval_offset >= 0);
                                assert(interval_offset + n_intervals <= d->n_intervals);
                                igSetCursorPos(V2(pos.x, scroll_y - 1.0f * y_scale));
                                ImPlot_PushColormap_PlotColormap(3);
                                ImPlot_PushStyleColor_U32(ImPlotCol_FrameBg, 0);
                                ImPlot_PushStyleColor_U32(ImPlotCol_PlotBorder, 0);
                                ImPlot_PushStyleColor_U32(ImPlotCol_PlotBg, 0);
                                ImPlot_SetNextPlotLimitsX(0, 1, 0);
                                ImPlot_SetNextPlotLimitsY((f64)scroll_y_time_lo, (f64)scroll_y_time_hi, ImGuiCond_Always, 0);
                                // ImPlot_SetLegendLocation(ImPlotLocation_East, ImPlotOrientation_Horizontal);
                                if (ImPlot_BeginPlot("idk", "p", "t", V2(0,(scroll_y_time_hi - scroll_y_time_lo) * y_scale),
                                  (ImPlotFlags_NoChild|ImPlotFlags_CanvasOnly) & ~ImPlotFlags_NoLegend,
                                  ImPlotAxisFlags_Lock|ImPlotAxisFlags_NoDecorations,
                                  ImPlotAxisFlags_Invert|ImPlotAxisFlags_Lock|ImPlotAxisFlags_NoGridLines, 0,0,0,0)) {
                                     ImPlot_PlotStairs_FloatPtrFloatPtr("a", d->interval_hand[0].pmod[0] + interval_offset, d->interval_times + interval_offset, n_intervals, 0, sizeof(float));
                                    ImPlot_EndPlot();
                                }
                                ImPlot_PopStyleColor(3);
                                ImPlot_PopColormap(1);
                            }
                            igEndChild();
                        }
                    }
                }

                igEndTabItem();
            }

            if (igBeginTabItem("Optimize", 0,0)) {
                f32 err_limit = 2.5f;
                f32 loss_limit = 1e-4f;
                for (isize i = 0; i < NumGraphSamples; i++) {
                    err_limit = max(err_limit, state.optimization_graph->ys[0][i]);
                    err_limit = max(err_limit, state.optimization_graph->ys[1][i]);
                    loss_limit = max(loss_limit, state.optimization_graph->ys[2][i]);
                }
                ImPlot_SetNextPlotLimitsX(state.opt.iter - NumGraphSamples, state.opt.iter, ImGuiCond_Always);
                ImPlot_SetNextPlotLimitsY(0, (f64)err_limit * 1.1, ImGuiCond_Always, 0);
                ImPlot_SetNextPlotLimitsY(0, (f64)loss_limit * 1.1, ImGuiCond_Always, 1);
                if (BeginPlotOptimizer("##OptGraph")) {
                    ImPlot_PlotLine_FloatPtrInt("Avg Error", state.optimization_graph->ys[0], state.optimization_graph->len, 1, state.opt.iter - NumGraphSamples, state.opt.iter + 1, sizeof(float));
                    ImPlot_PlotLine_FloatPtrInt("Max Positive Error", state.optimization_graph->ys[1], state.optimization_graph->len, 1, state.opt.iter - NumGraphSamples, state.opt.iter + 1, sizeof(float));
                    ImPlot_SetPlotYAxis(ImPlotYAxis_2);
                    ImPlot_PlotLine_FloatPtrInt("Loss", state.optimization_graph->ys[2], state.optimization_graph->len, 1, state.opt.iter - NumGraphSamples, state.opt.iter + 1, sizeof(float));
                    ImPlot_EndPlot();
                }

                if (igButton(state.optimizing ? "Stop" : "Start", V2Zero)) {
                    state.optimizing = !state.optimizing;
                }
                igSameLine(0, 4);
                if (igButton("Checkpoint", V2Zero)) {
                    checkpoint();
                }
                if (optimizer_error_message) {
                    igSameLine(0, 4);
                    igTextUnformatted(optimizer_error_message, 0);
                }

                if (igBeginChild_Str("Hyperparameters", V2(GetContentRegionAvailWidth() / 2.0f, 250.0f), 0, 0)) {
                    igSliderFloat("h", &H, 1.0e-8f, 0.1f, "%g", 1.0f);
                    tooltip("coarseness of the derivative approximation\n\nfinite differences baybee");
                    igSliderFloat("Step Size", &StepSize, 1.0e-8f, 0.1f, "%g", ImGuiSliderFlags_None);
                    tooltip("how fast to change parameters. large values can be erratic");
                    igSliderInt("Sample Batch Size", &SampleBatchSize, 1, maxs(1, state.opt.n_samples), "%d", ImGuiSliderFlags_AlwaysClamp);
                    tooltip("random sample of n files for each step");
                    igSliderInt("Parameter Batch Size", &ParameterBatchSize, 1, maxs(1, state.opt.n_params), "%d", ImGuiSliderFlags_AlwaysClamp);
                    tooltip("random sample of n parameters for each step");
                    igSliderFloat("Skillset/Overall Balance", &SkillsetOverallBalance, 0.0f, 1.0f, "%f", ImGuiSliderFlags_AlwaysClamp);
                    tooltip("0 = train only on skillset\n1 = train only on overall");
                    igSliderFloat("Misclass Penalty", &Misclass, 0.0f, 5.0f, "%f", ImGuiSliderFlags_AlwaysClamp);
                    tooltip("exponentially increases loss proportional to (largest_skillset_ssr - target_skillset_ssr)");
                    igSliderFloat("Exp Scale", &ExpScale, 0.0f, 1.0f, "%f", ImGuiSliderFlags_AlwaysClamp);
                    tooltip("weights higher MSDs heavier automatically\n0 = train on msd\n1 = train on exp(msd)\n\nmsd is zero centered and normalized so dw about it");
#if 0
                    igSliderFloat("Underrated dead zone", &NegativeEpsilon, 0.0f, 10.0f, "%f", 1.0f);
                    tooltip("be more accepting of files that come under their target ssr than over");
                    NegativeEpsilon = UnderratedDeadZone / state.target.msd_sd;
#endif
                    igSliderFloat("Regularisation", &Regularisation, 0.0f, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
                    tooltip("penalise moving parameters very far away from the defaults");
                    igSliderFloat("Regularisation Alpha", &RegularisationAlpha, 0.0f, 1.0f, "%f", ImGuiSliderFlags_None);
                    tooltip("0 = prefer large changes to few parameters\n1 = prefer small changes to many parameters\n...theoretically");
                }
                igEndChild();
                igSameLine(0, 4);


                if (igBeginChild_Str("Checkpoints", V2(GetContentRegionAvailWidth(), 250.0f), 0, 0)) {
                    if (igSelectable_Bool(state.latest_checkpoint.name, 0, ImGuiSelectableFlags_None, V2Zero)) {
                        restore_checkpoint(state.latest_checkpoint);
                    }
                    if (igSelectable_Bool(state.default_checkpoint.name, 0, ImGuiSelectableFlags_None, V2Zero)) {
                        restore_checkpoint(state.default_checkpoint);
                    }
                    for (isize i = buf_len(state.checkpoints) - 1; i >= 0; i--) {
                        if (igSelectable_Bool(state.checkpoints[i].name, 0, ImGuiSelectableFlags_None, V2Zero)) {
                            restore_checkpoint(state.checkpoints[i]);
                        }
                    }
                }
                igEndChild();

#if 0
                for (isize i = 1; i < NumSkillsets; i++) {
                    igPushIDInt((i32)i);
                    igText("%s Bias", SkillsetNames[i]);
                    tooltip("+ offsets the target MSD for this skillset\n* multiplies\n%% sets the target wife%%");
                    igSameLine(0, 4);
                    igSetCursorPosX(116.f);
                    TextString("+", 0));
                    igSameLine(0,4);
                    igSetNextItemWidth(igGetWindowContentRegionWidth() / 5.0f);
                    igSliderFloat("*##+", &state.opt_cfg.bias[i].add, -5.0f, 5.0f, "%f", 1.0f);
                    igSameLine(0, 4);
                    igSetNextItemWidth(igGetWindowContentRegionWidth() / 5.0f);
                    igSliderFloat("^##*", &state.opt_cfg.bias[i].mul, 0.1f, 5.0f, "%f", 1.0f);
                    igSameLine(0, 4);
                    igSetNextItemWidth(igGetWindowContentRegionWidth() / 5.0f);
                    igSliderFloat("##%", &state.opt_cfg.goals[i], 0.9f, 0.965f, "%f", 1.0f);
                    igPopID();
                }
#endif
                igEndTabItem();
            }
            igEndTabBar();
        }
    }
    igEnd();

    // Active files list
    right_width = ds.x - (left_width + centre_width);
    igSetNextWindowSizeConstraints(V2(0, -1), V2(FLT_MAX, -1), 0, 0);
    igSetNextWindowPos(V2(ds.x - right_width, 0.0f), ImGuiCond_Always, V2Zero);
    igSetNextWindowSize(V2(right_width + 1.0f, ds.y + 1.0f), ImGuiCond_Always);
    if (igBegin("Files", 0, ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize)) {
        // Header + arrow drop down fake thing
        if (state.target.average_delta.E[0] != 0.0f) {
            for (isize i = 0; i < NumSkillsets; i++) {
                igTextColored(msd_color(absolute_value(state.target.average_delta.E[i]) * 3.0f), "%02.2f", (f64)state.target.average_delta.E[i]);
                tooltip("Optimizer: %s abs delta", SkillsetNames[i]);
                if (i == 0) {
                    igSameLine(0, 0); TextString(S(" (")); igSameLine(0, 0);
                    igTextColored(msd_color(absolute_value(state.target.min_delta) * 3.0f), "%02.2f", (f64)state.target.min_delta);
                    tooltip("Optimizer: min abs delta");
                    igSameLine(0, 0); TextString(S(", ")); igSameLine(0 ,0);
                    igTextColored(msd_color(absolute_value(state.target.max_delta) * 3.0f), "%02.2f", (f64)state.target.max_delta);
                    tooltip("Optimizer: max abs delta");
                    igSameLine(0, 0); TextString(S(")")); igSameLine(0, 0);
                }
                if (i != (NumSkillsets - 1)) {
                    igSameLine(0, 12.0f);
                }
            }
        }
        igSeparator();

        // Open file list
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (sfi->target.weight > 0) {
                f32 wife93 = sfi->aa_rating.overall;
                f32 wife965 = sfi->max_rating.overall;
                if (sfi->target.want_msd) {
                    igTextColored(msd_color(absolute_value(sfi->target.delta) * 3.0f), "%s%02.2f", sfi->target.delta >= 0.0f ? " " : "", (f64)sfi->target.delta);
                    tooltip("Optimizer: %02.2f %s, want %02.2f at %02.2fx", (f64)sfi->target.got_msd, SkillsetNames[sfi->target.skillset], (f64)sfi->target.want_msd, (f64)sfi->target.rate); igSameLine(0, 7.0f);
                }
                igTextColored(msd_color(wife93), "%02.2f", (f64)wife93);
                tooltip("Overall at AA"); igSameLine(0, 7.0f);
                igTextColored(msd_color(wife965), "%02.2f", (f64)wife965);
                tooltip("Overall at max scaling"); igSameLine(0, 7.0f);
                igPushID_Str(sfi->id.buf);
                if (sfi == active) {
                    igPushStyleColor_Vec4(ImGuiCol_Header, msd_color(sfi->aa_rating.overall));
                }
                if (igSelectable_Bool(sfi->id.buf, sfi->open, ImGuiSelectableFlags_None, V2Zero)) {
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
                igPopID();
                igSeparator();
            }
        }
    }
    igEnd();

    // Debug window
    if (igIsKeyPressed('`', false)) {
        DUMP_CONSTANT_INFO;
    }
    {
        debug_counters.skipped = 0;
        debug_counters.done = 0;
        f64 time = 0;
        for (CalcThread *ct = state.threads; ct != buf_end(state.threads); ct++) {
            debug_counters.skipped += ct->debug_counters.skipped;
            debug_counters.done += ct->debug_counters.done;
            time += ct->debug_counters.time;
        }
        time = 1000. * time / (f64)buf_len(state.threads);

        igSetNextWindowSize(V2(centre_width - 4.0f, 0), 0);
        igSetNextWindowPos(V2(left_width + 2.f, ds.y - 2.f), 0, V2(0, 1));
        igBegin("Debug", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

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
        igBeginGroup(); igText("Average calc time per thousand non-empty rows"); igText("%02.2fms", time); igEndGroup();

        igEnd();
    }

    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();

    state.last_frame.left_width = left_width;
    state.last_frame.right_width = right_width;

    // Restore last interacted-with file on close
    if (active->open == false) {
        next_active = null_sfi;
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (sfi->open && sfi->frame_last_focused >= next_active->frame_last_focused) {
                next_active = sfi;
            }
        }
    }

    if (next_active != null_sfi) {
        // Allow one file to be open "in the background", and don't free its
        // graphs. For for any older files, free all their parameter graphs.
        // Combined with the file restore above, this lets you to A/B between
        // two files.
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (sfi->open && sfi != active && sfi != next_active) {
                sfi->open = false;
                if (buf_len(sfi->graphs) > 1) {
                    for (isize i = 1; i < buf_len(sfi->graphs); i++) {
                        free_graph(sfi->graphs[i]);
                    }
                    buf_set_len(sfi->graphs, 1);
                }
            }
        }
        if (buf_len(next_active->graphs) == 1) {
            for (isize i = 1; i < buf_len(active->graphs); i++) {
                assert(i <= buf_len(state.parameter_graph_order));
                buf_push(next_active->graphs, make_parameter_graph(state.parameter_graph_order[i-1]));
            }
        }
        assert(next_active->open);
        next_active->frame_last_focused = _sapp.frame_count;
        state.active = next_active;
        calculate_file_graphs(&state.high_prio_work, state.active, state.generation);
        calculate_effects(&state.high_prio_work, &state.info, state.active, false, state.generation);
    }

    if (changed_param.type) {
        switch (changed_param.type) {
            case ParamSlider_GraphToggled: {
                if (state.parameter_graphs_enabled[changed_param.param]) {
                    buf_push(state.parameter_graph_order, changed_param.param);
                    if (state.active) {
                        buf_push(state.active->graphs, make_parameter_graph(changed_param.param));
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

    submit_work(&high_prio_work_queue, state.debug_graph_work, state.generation);
    submit_work(&high_prio_work_queue, state.high_prio_work, state.generation);
    submit_work(&low_prio_work_queue, state.low_prio_work, state.generation);
    fflush(0);
}

void cleanup(void)
{
    simgui_shutdown();
    sg_shutdown();
}

#if defined(__EMSCRIPTEN__)
static void emsc_load_callback(const sapp_html5_fetch_response *response) {
    if (response->succeeded) {
        buf_push(state.dropped_files, (Buffer) {
            .buf = response->buffer_ptr,
            .len = response->fetched_size,
            .cap = response->buffer_size
        });
    } else {
        // todo
    }
}
#endif

void input(const sapp_event* event)
{
    simgui_handle_event(event);
    if (event->type == SAPP_EVENTTYPE_FILES_DROPPED) {
        int n = sapp_get_num_dropped_files();
        for (int i = 0; i < n; i++) {
#if defined(__EMSCRIPTEN__)
            uint32_t size = sapp_html5_get_dropped_file_size(i);
            sapp_html5_fetch_dropped_file(&(sapp_html5_fetch_request){
                .dropped_file_index = i,
                .callback = emsc_load_callback,
                .buffer_ptr = malloc(size),
                .buffer_size = size,
            });
#else
            char const *path = sapp_get_dropped_file_path(i);
            buf_push(state.dropped_files, read_file_malloc(path));
#endif
        }
    }
}

sapp_desc sokol_main(int argc, char **argv)
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
        .ios_keyboard_resizes_canvas = false,
        .enable_clipboard = true,
        .enable_dragndrop = true,
        .max_dropped_files = 128,
        .icon.sokol_default = true,
    };
}
