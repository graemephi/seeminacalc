#if !(defined(SOKOL_GLCORE33)||defined(SOKOL_GLES2)||defined(SOKOL_GLES3)||defined(SOKOL_D3D11)||defined(SOKOL_METAL)||defined(SOKOL_WGPU)||defined(SOKOL_DUMMY_BACKEND))
#define SOKOL_GLCORE33
#endif

#ifdef __clang__
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

#define TEST_CHARTKEYS 0

#if TEST_CHARTKEYS
#include "sqlite3.c"
// The perils of including sqlite3 in your translation unit
// sqlite3.c also disables certain warnings.
#if defined(_DEBUG) && NDEBUG == 1
#undef NDEBUG
#endif
#endif

#include "thread.h"
#include "bottom.h"
#include "cminacalc.h"
#include "sm.h"

#pragma float_control(precise, on, push)
#include "sm.c"
#pragma float_control(pop)

#ifdef SQLITE_CORE
typedef struct CacheDB
{
    sqlite3 *db;
    sqlite3_stmt *note_data_stmt;
} CacheDB;

static CacheDB cache_db = {0};

CacheDB cachedb_init(const char *path)
{
    CacheDB result;
    int rc = sqlite3_open(path, &result.db);
    if (rc) {
        goto err;
    }

    char query[] = "select serializednotedata from steps where chartkey=?;";
    rc = sqlite3_prepare_v2(result.db, query, sizeof(query), &result.note_data_stmt, 0);
    if (rc) {
        goto err;
    }

    return result;

err:
    fprintf(stderr, "Sqlite3 oops: %s\n", sqlite3_errmsg(result.db));
    sqlite3_close(result.db);
    result = (CacheDB) {0};
    return result;
}

Buffer get_steps_from_db(CacheDB *db, char *key)
{
    Buffer result = (Buffer) {0};
    sqlite3_bind_text(db->note_data_stmt, 1, key, -1, 0);
    sqlite3_step(db->note_data_stmt);
    const void *blob = sqlite3_column_blob(db->note_data_stmt, 0);
    size_t len = sqlite3_column_bytes(db->note_data_stmt, 0);

    result.buf = calloc(len, sizeof(char));
    memcpy(result.buf, blob, len);
    result.len = len;
    result.cap = len;

    sqlite3_reset(db->note_data_stmt);
    return result;
}
#endif

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

u64 rng(void)
{
    // wikipedia
	static usize x = 1;
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	return x * 0x2545F4914F6CDD1DULL;
}

f32 rngf(void)
{
    u32 a = rng() & ((1 << 23) - 1);
    return (f32)a / (f32)(1 << 23);
}

static const ImVec2 V2Zero = {0};

bool BeginPlotCppCpp(const char* title, const char* x_label, const char* y_label, const ImVec2* size, ImPlotFlags flags, ImPlotAxisFlags x_flags, ImPlotAxisFlags y_flags, ImPlotAxisFlags y2_flags, ImPlotAxisFlags y3_flags);
static bool BeginPlotDefaults(const char* title_id, const char* x_label, const char* y_label)
{
    return BeginPlotCppCpp(title_id, x_label, y_label, &(ImVec2){igGetWindowWidth() / 2.0f - 8.0f, 0}, ImPlotFlags_Default & ~ImPlotFlags_Legend, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary);
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
    // 	if x then
    // 		return HSV(math.max(95 - (x / 40) * 150, -50), 0.9, 0.9)
    // 	end
    // 	return HSV(0, 0.9, 0.9)
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

    bool debug_window;
} State;
static State state = {0};

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

#include "graphs.c"

i32 make_skillsets_graph()
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
        String diff = DifficultyStrings[sm.diffs[i].diff];
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
        calculate_file_graph_force(&state.low_prio_work, sfi, &state.graphs[sfi->graphs[0]], state.generation);

#if TEST_CHARTKEYS
        Buffer b = get_steps_from_db(&cache_db, sfi->chartkey.buf);
        assert(b.len);
#endif

        printf("Added %s\n", id.buf);
    }

    submit_work(&high_prio_work_queue, state.high_prio_work, state.generation);

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

#ifdef __EMSCRIPTEN__
    const char *files[0] = {};
#else
    const char *files[] = {
        "./Odin.sm",
        "./osu.sm",
        "./03 IMAGE -MATERIAL-(Version 0).sm",
        "./Grief & Malice.sm",
        "./Skycoffin CT.sm",
        "./The Lost Dedicated Life.sm",
        "./m1dy - 960 BPM Speedcore.sm",
        "./alien temple.sm",

        // Junk """""""test vectors"""""""
        "./03 IMAGE -MATERIAL-(Version 0).sm",
        "./seeminacalc.c",
        "web/NotoSansCJKjp-Regular.otf"
    };
#endif

    state.generation = 1;
    state.info = calc_info();
    state.calc = calc_init(&state.info);
    state.ps = copy_param_set(&state.info.defaults);
    buf_pushn(state.parameter_graphs_enabled, state.info.num_params);
    buf_reserve(state.graphs, 128);

    // todo: use handles instead
    buf_reserve(state.files, 1024);

    high_prio_work_queue.lock_id = make_lock();
    low_prio_work_queue.lock_id = make_lock();

    i32 n_threads = got_any_cores() - 1;
    for (isize i = 0; i < n_threads; i++) {
        CalcThread *ct = buf_push(state.threads, (CalcThread) {
            .info = &state.info,
            .generation = &state.generation,
            .ps = &state.ps,
            .done = buf_pushn(done_queues, 1)
        });

        make_thread(calc_thread, ct);
    }

    for (isize i = 0; i < array_length(files); i++) {
        Buffer f = read_file(files[i]);
        parse_and_add_sm(f, i == 0);
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
        igTextUnformatted("github", 0);
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

    ParamSliderChange changed = {0};
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
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if (active->effects.strong == 0 || (active->effects.strong[mp] & effect_mask) != 0) {
                                param_slider_widget(mp, show_parameter_names, &changed);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            } tooltip("These will basically always change the MSD of the active file's selected skillsets");
            if (igBeginTabItem("Relevant", 0, ImGuiTabItemFlags_None)) {
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if (active->effects.weak == 0 || (active->effects.weak[mp] & effect_mask) != 0) {
                                param_slider_widget(mp, show_parameter_names, &changed);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            } tooltip("More, plus some params that need more shoving");
            if (igBeginTabItem("All", 0, ImGuiTabItemFlags_None)) {
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            param_slider_widget(mp, show_parameter_names, &changed);
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            } tooltip("Everything\nPretty useless unless you like finding out which knobs do nothing yourself");
            if (igBeginTabItem("Dead", 0, ImGuiTabItemFlags_None)) {
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if (active->effects.weak && (active->effects.weak[mp] & effect_mask) == 0) {
                                param_slider_widget(mp, show_parameter_names, &changed);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            } tooltip("These don't do anything to the active file's selected skillsets");
        }
        igEndTabBar();
    }
    igEnd();

    if (changed.type == ParamSlider_ValueChanged) {
        state.generation++;
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
                calculate_effects(&state.low_prio_work, &state.info, sfi, state.generation);

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
                    if (fng->param == changed.param && (changed.type == ParamSlider_LowerBoundChanged || changed.type == ParamSlider_UpperBoundChanged)) {
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

    if (buf_len(state.files) == 0) {
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
        time /= (f64)buf_len(state.threads);

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
        igBeginGroup(); igText("Average calc time per thousand non-empty rows"); igText("%.2fms", time * 1000.); igEndGroup();

        igEnd();
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

    if (changed.type) {
        switch (changed.type) {
            case ParamSlider_GraphToggled: {
                if (state.parameter_graphs_enabled[changed.param]) {
                    buf_push(state.parameter_graph_order, changed.param);
                    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                        buf_push(sfi->graphs, make_parameter_graph(changed.param));
                    }
                } else {
                    for (isize i = 0; i < buf_len(state.parameter_graph_order); i++) {
                        if (state.parameter_graph_order[i] == changed.param) {
                            buf_remove_sorted_index(state.parameter_graph_order, i);
                            break;
                        }
                    }

                    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                        for (isize i = 0; i != buf_len(sfi->graphs); i++) {
                            if (state.graphs[sfi->graphs[i]].param == changed.param) {
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
    // This lets you sweep param sliders without the first graphs
    // stealing all the work
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

#if TEST_CHARTKEYS
    assert(argc == 2);
    cache_db = cachedb_init(argv[1]);
#endif

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
