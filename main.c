#if !(defined(SOKOL_GLCORE33)||defined(SOKOL_GLES2)||defined(SOKOL_GLES3)||defined(SOKOL_D3D11)||defined(SOKOL_METAL)||defined(SOKOL_WGPU)||defined(SOKOL_DUMMY_BACKEND))
#define SOKOL_GLCORE33
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

#define TEST_CHARTKEYS 0

#if TEST_CHARTKEYS
#include "sqlite3.c"
// The perils of including sqlite3 in your translation unit
// sqlite3.c also disables certain warnings.
#if defined(_DEBUG) && NDEBUG == 1
#undef NDEBUG
#endif
#endif

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
    Buffer buf = (Buffer) {
        .buf = alloc(u8, len),
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

u64 rng()
{
    // wikipedia
	static usize x = 1;
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	return x * 0x2545F4914F6CDD1DULL;
}

f32 rngf()
{
    u32 a = rng() & ((1 << 23) - 1);
    return a / (f32)(1 << 23);
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

typedef struct Fn
{
    f32 xs[1024];
    f32 ys[1024];
} Fn;

typedef struct FnGraph FnGraph;
struct FnGraph
{
    b32 active;
    b32 is_param;
    i32 param;
    i32 len;

    Fn absolute[NumSkillsets];
    f32 min;
    f32 max;

    Fn relative[NumSkillsets];
    f32 relative_min;
};
// 128KB

typedef struct SimFileInfo
{
    String title;
    String diff;
    String chartkey;
    String id;

    NoteData *notes;
    EffectMasks effects;

    f32 *skillsets_over_wife[NumSkillsets];
    f32 *relative_skillsets_over_wife[NumSkillsets];
    f32 min_rating;
    f32 max_rating;
    f32 min_relative_rating;
    f32 max_relative_rating;

    i32 *graphs;

    bool selected_skillsets[NumSkillsets];
    bool display_skillsets[NumSkillsets];

    bool open;
    bool stops;
    u64 frame_last_focused;
} SimFileInfo;

static SimFileInfo null_sfi_ = {0};
static SimFileInfo *const null_sfi = &null_sfi_;

typedef struct State
{
    u64 last_time;
    sg_pass_action pass_action;

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
    i32 num_open_windows;
    b32 parameters_shown_last_frame;
} State;
static State state = {0};

float ssxs[19];

i32 parse_and_add_sm(Buffer buf, b32 open_window)
{
    push_allocator(scratch);
    SmFile sm = {0};
    i32 err = parse_sm(buf, &sm);
    if (err) {
        pop_allocator();
        return -1;
    }

    NoteInfo *ni = sm_to_ett_note_info(&sm, 0);
    String ck = generate_chart_key(&sm, 0);
    String title = sm_tag_inplace(&sm, Tag_Title);
    String author = sm_tag_inplace(&sm, Tag_Credit);
    String diff = DifficultyStrings[sm.diffs[0].diff];
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
        .open = open_window
    });

    buf_reserve(sfi->effects.weak, state.info.num_params);
    buf_reserve(sfi->effects.strong, state.info.num_params);
    // calculate_effects(&state.info, &state.calc, sfi->notes, &sfi->effects);

#if TEST_CHARTKEYS
    Buffer b = get_steps_from_db(&cache_db, sfi->chartkey.buf);
    assert(b.len);
#endif

    sfi->min_rating = 40;
    sfi->min_relative_rating = 2;
    for (i32 x = 0; x < 19; x++) {
        f32 goal = 0.82f + ((f32)x / 100.f);
        ssxs[x] = goal * 100.f;
        SkillsetRatings ssr;
        calc_go(&state.calc, &state.info.defaults, sfi->notes, 1.0f, goal, &ssr);
        for (i32 r = 0; r < NumSkillsets; r++) {
            buf_push(sfi->skillsets_over_wife[r], ssr.E[r]);
            sfi->min_rating = min(ssr.E[r], sfi->min_rating);
            sfi->max_rating = max(ssr.E[r], sfi->max_rating);

            f32 relative_rating = ssr.E[r] / ssr.overall;
            buf_push(sfi->relative_skillsets_over_wife[r], relative_rating);
            sfi->min_relative_rating = min(relative_rating, sfi->min_relative_rating);
            sfi->max_relative_rating = max(relative_rating, sfi->max_relative_rating);
        }
    }

    for (isize ss = 1; ss < NumSkillsets; ss++) {
        sfi->display_skillsets[ss] = (0.9 <= (sfi->skillsets_over_wife[ss][18] / sfi->skillsets_over_wife[0][18]));
        sfi->selected_skillsets[ss] = sfi->display_skillsets[ss];
    }

    printf("Added %s\n", id.buf);
    return 0;
}

i32 make_parameter_graph(i32 param)
{
    FnGraph *fg = 0;
    if (buf_len(state.free_graphs) > 0) {
        fg = &state.graphs[buf_pop(state.free_graphs)];
    } else {
        fg = buf_pushn(state.graphs, 1);
    }
    fg->active = true;
    fg->is_param = true;
    fg->param = param;
    fg->len = state.info.params[param].integer ? (i32)state.info.params[param].max : 10;
    fg->min = FLT_MAX;
    fg->max = FLT_MIN;
    fg->relative_min = FLT_MAX;
    return (i32)buf_index_of(state.graphs, fg);
}

void free_parameter_graph(i32 handle)
{
    state.graphs[handle].active = false;
    buf_push(state.free_graphs, handle);
}

void free_all_parameter_graphs(i32 handles[])
{
    for (i32 i = 0; i != buf_len(handles); i++) {
        i32 handle = handles[i];
        state.graphs[handle].active = false;
        buf_push(state.free_graphs, handle);
    }
    buf_clear(handles);
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

void init(void)
{
    isize bignumber = 100*1024*1024;
    scratch_stack = stack_make(malloc(bignumber), bignumber);
    permanent_memory_stack = stack_make(malloc(bignumber), bignumber);

    push_allocator(scratch);
    Buffer font = load_font_file("NotoSansCJKjp-Regular.otf");
    pop_allocator();

    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    stm_setup();
    simgui_setup(&(simgui_desc_t){
        .no_default_font = (font.buf != 0),
        .sample_count = _sapp.sample_count
    });
    state = (State) {
        .pass_action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f }
            }
        }
    };

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
        "./osu.sm",
        "./03 IMAGE -MATERIAL-(Version 0).sm",
        "./Grief & Malice.sm",
        "./Odin.sm",
        "./Skycoffin CT.sm",
        "./The Lost Dedicated Life.sm",
        "./m1dy - 960 BPM Speedcore.sm",
        "./alien temple.sm",

        // Junk """""""test vectors"""""""
        "./03 IMAGE -MATERIAL-(Version 0).sm",
        "./main.c",
        "NotoSansCJKjp-Regular.otf"
    };
#endif

    state.info = calc_info();
    state.calc = calc_init(&state.info);
    state.ps = copy_param_set(&state.info.defaults);
    buf_pushn(state.parameter_graphs_enabled, state.info.num_params);
    buf_reserve(state.graphs, 128);

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

static i32 param_slider_widget(i32 param_idx, b32 show_parameter_names)
{
    b32 toggled = false;
    i32 mp = param_idx;
    igPushIDInt(mp);
    if (igCheckbox("##graph", &state.parameter_graphs_enabled[mp])) {
        toggled = true;
    } tooltip("graph this parameter");
    igSameLine(0, 4);
    char slider_id[32];
    snprintf(slider_id, sizeof(slider_id), "##slider%d", mp);
    if (state.info.params[mp].integer) {
        i32 value = (i32)state.ps.params[mp];
        i32 low = (i32)state.ps.min[mp];
        i32 high = (i32)state.ps.max[mp];
        igSliderInt(slider_id, &value, low, high, "%d");
        state.ps.params[mp] = (f32)value;
    } else {
        f32 speed = (state.ps.max[mp] - state.ps.min[mp]) / 100.f;
        igSetNextItemWidth(igGetFontSize() * 10.0f);
        igSliderFloat(slider_id, &state.ps.params[mp], state.ps.min[mp], state.ps.max[mp], "%f", 1.0f);
        if (show_parameter_names == false) {
            tooltip(state.info.params[mp].name);
        }
        if (ItemDoubleClicked(0)) {
            state.ps.params[mp] = state.info.defaults.params[mp];
        }
        igSameLine(0, 4);
        igSetNextItemWidth(igGetFontSize() * 4.0f);
        igDragFloatRange2("##range",  &state.ps.min[mp], &state.ps.max[mp], speed, -100.0f, 100.0f, "%.1f", "%.1f", 1.0f);
        tooltip("min/max override (the defaults are guesses)");
        if (ItemDoubleClicked(0)) {
            ImVec2 l, u;
            igGetItemRectMin(&l);
            igGetItemRectMax(&u);
            u.x = l.x + (u.x - l.x) * 0.5f;;
            b32 left = igIsMouseHoveringRect(l, u, true);
            if (left) {
                state.ps.min[mp] = state.info.defaults.min[mp];
            } else {
                state.ps.max[mp] = state.info.defaults.max[mp];
            }
        }
    }
    if (show_parameter_names) {
        igSameLine(0, 0);
        igText(state.info.params[mp].name);
    }
    igPopID();
    return toggled;
}

static void skillset_line_plot(i32 ss, b32 highlight, f32 *xs, f32 *ys, i32 count)
{
    // Recreate highlighting cause ImPlot doesn't let you tell it to highlight lines
    if (highlight) {
        ipPushStyleVarFloat(ImPlotStyleVar_LineWeight, ipGetStyle()->LineWeight * 2.0f);
    }

    ipPushStyleColorU32(ImPlotCol_Line, state.skillset_colors[ss]);
    ipPlotLineFloatPtrFloatPtr(SkillsetNames[ss], xs, ys, count, 0, sizeof(float));
    ipPopStyleColor(1);

    if (highlight) {
        ipPopStyleVar(1);
    }
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

void frame(void)
{
    reset_scratch();

    i32 width = sapp_width();
    i32 height = sapp_height();
    f64 delta_time = stm_sec(stm_laptime(&state.last_time));
    simgui_new_frame(width, height, delta_time);

    bool ss_highlight[NumSkillsets] = {0};
    SimFileInfo *next_active = 0;

    ImGuiIO *io = igGetIO();
    ImVec2 ds = io->DisplaySize;

    ImGuiWindowFlags fixed_window = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    f32 right_width = 400.0f;
    igSetNextWindowPos((ImVec2) { ds.x - right_width, 0.0f }, ImGuiCond_Always, V2Zero);
    igSetNextWindowSize((ImVec2) { right_width + 1.0f, ds.y + 1.0f }, ImGuiCond_Always);
    if (igBegin("Files", 0, fixed_window)) {
        // Header + arrow drop down fake thing
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
            igTextColored(msd_color(sfi->skillsets_over_wife[0][13]), "%2.2f", sfi->skillsets_over_wife[0][13]);
            tooltip("Overall at AA"); igSameLine(0, 7.0f);
            igTextColored(msd_color(sfi->skillsets_over_wife[0][18]), "%2.2f", sfi->skillsets_over_wife[0][18]);
            tooltip("Overall at max scaling"); igSameLine(0, 7.0f);
            igPushIDStr(sfi->id.buf);
            if (igSelectableBool(sfi->id.buf, sfi->open, ImGuiSelectableFlags_None, V2Zero)) {
                if (sfi->open) {
                    sfi->open = false;
                } else {
                    sfi->open = true;
                    next_active = sfi;
                }
            }
            if (sfi->stops) {
                igSameLine(igGetWindowWidth() - 30.f, 4);
                igTextColored((ImVec4) { 0.85f, 0.85f, 0.0f, 0.95f }, "  !  ");
                tooltip("This file has stops or negBPMs. These are parsed differently from Etterna, so the ratings will differ from what you see in Ettera.\n\n"
                        "Note that the calc is VERY sensitive to tiny variations in ms row times.");
            }
            if (skillsets) {
                for (isize ss = 0; ss < NumSkillsets; ss++) {
                    char r[32] = {0};
                    snprintf(r, sizeof(r), "%2.2f##%d", sfi->skillsets_over_wife[ss][13], (i32)ss);
                    igPushStyleColorU32(ImGuiCol_Header, state.skillset_colors_selectable[ss]);
                    igPushStyleColorU32(ImGuiCol_HeaderHovered, state.skillset_colors[ss]);
                    igSelectableBool(r, sfi->display_skillsets[ss], ImGuiSelectableFlags_None, (ImVec2) { 300.0f / NumSkillsets, 0 });
                    tooltip("%s", SkillsetNames[ss]);
                    if (igIsItemHovered(0)) {
                        ss_highlight[ss] = 1;
                    }
                    igPopStyleColor(2);
                    igSameLine(0, 4);
                    igDummy((ImVec2){4,0});
                    igSameLine(0, 4);
                }
                igNewLine();
            }
            igPopID();
            igSeparator();
        }
    }
    igEnd();

    SimFileInfo *active = state.active;
    i32 param_toggled = -1;

    f32 left_width = state.parameters_shown_last_frame ? 450.0f : 300.0f;
    igSetNextWindowPos((ImVec2) { -1.0f, 0 }, ImGuiCond_Always,  V2Zero);
    igSetNextWindowSize((ImVec2) { left_width + 1.0f, ds.y + 1.0f }, ImGuiCond_Always);
    if (igBegin("Mod Parameters", 0, fixed_window)) {
        u32 effect_mask = 0;
        isize clear_selections_to = -1;
        igPushIDStr(active->id.buf);

        // Skillset filters
        f32 selectable_width_factor = 4.0f;
        for (isize i = 0; i < NumSkillsets; i++) {
            igPushStyleColorU32(ImGuiCol_Header, state.skillset_colors_selectable[i]);
            igPushStyleColorU32(ImGuiCol_HeaderHovered, state.skillset_colors[i]);
            igSelectableBoolPtr(SkillsetNames[i], &active->selected_skillsets[i], 0, (ImVec2){ (igGetWindowWidth() - 12*4)  / selectable_width_factor, 0});
            igPopStyleColor(2);
            if (ItemDoubleClicked(0)) {
                clear_selections_to = i;
            }
            if (igIsItemHovered(0)) {
                ss_highlight[i] = 1;
            }
            if (i != 3) {
                igSameLine(0, 4);
                igDummy((ImVec2){4, 4});
                igSameLine(0, 4);
            } else {
                selectable_width_factor = 4.0f;
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
            b32 show_parameter_names = false;
            if (igTreeNodeExStr("##parameter name toggle", ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
                show_parameter_names = true;
            } tooltip("show parameter names");
            igPopStyleColor(2);
            state.parameters_shown_last_frame = show_parameter_names;

            // The actual tabs
            if (igBeginTabItem("More Relevant", 0, ImGuiTabItemFlags_None)) {
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if (active->effects.strong == 0 || (active->effects.strong[mp] & effect_mask) != 0) {
                                if (param_slider_widget(mp, show_parameter_names)) {
                                    param_toggled = mp;
                                }
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
                                if (param_slider_widget(mp, show_parameter_names)) {
                                    param_toggled = mp;
                                }
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
                            if (param_slider_widget(mp, show_parameter_names)) {
                                param_toggled = mp;
                            }
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
                                if (param_slider_widget(mp, show_parameter_names)) {
                                    param_toggled = mp;
                                }
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

    // MSD graph windows
    i32 num_open_windows = 0;
    f32 centre_width = ds.x - left_width - right_width;
    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
        if (sfi->open) {
            if (state.num_open_windows == 0 && centre_width >= 450.0f) {
                igSetNextWindowPos((ImVec2) { left_width, 0 }, ImGuiCond_Always, V2Zero);
                igSetNextWindowSize((ImVec2) {centre_width , ds.y }, ImGuiCond_Always);
            } else {
                ImVec2 sz = (ImVec2) { clamp_high(ds.x, 750.0f), clamp(300.0f, ds.y, ds.y * 0.33f * (buf_len(sfi->graphs) + 1)) };
                ImVec2 pos = (ImVec2) { rngf() * (ds.x - sz.x), rngf() * (ds.y - sz.y) };
                igSetNextWindowPos(pos, ImGuiCond_Appearing, V2Zero);
                igSetNextWindowSize(sz, ImGuiCond_Appearing);
            }
            if (igBegin(sfi->id.buf, &sfi->open, ImGuiWindowFlags_None)) {
                if (igIsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                    next_active = sfi;
                }
                igText(sfi->diff.buf);
                igSameLine(clamp_low(igGetWindowWidth() - 268.f, 100), 0);
                igText(sfi->chartkey.buf);

                ipSetNextPlotLimits(82, 100, sfi->min_rating - 1.f, sfi->max_rating + 2.f, ImGuiCond_Once);
                if (BeginPlotDefaults("Rating", "Wife%", "SSR")) {
                    for (i32 r = 0; r < NumSkillsets; r++) {
                        skillset_line_plot(r, ss_highlight[r], ssxs, sfi->skillsets_over_wife[r], 19);
                    }
                    ipEndPlot();
                }
                igSameLine(0, 4);
                ipSetNextPlotLimits(82, 100, sfi->min_relative_rating - 0.05f, sfi->max_relative_rating + 0.05f, ImGuiCond_Once);
                if (BeginPlotDefaults("Relative Rating", "Wife%", 0)) {
                    for (i32 r = 1; r < NumSkillsets; r++) {
                        skillset_line_plot(r, ss_highlight[r], ssxs, sfi->relative_skillsets_over_wife[r], 19);
                    }
                    ipEndPlot();
                }

                for (i32 fgi = (i32)buf_len(sfi->graphs) - 1; fgi >= 0; fgi--) {
                    FnGraph *fg = &state.graphs[sfi->graphs[fgi]];
                    i32 mp = fg->param;
                    ParamInfo *p = &state.info.params[mp];
                    if (fg->max == FLT_MIN) {
                        break;
                    }
                    u8 full_name[64] = {0};
                    snprintf(full_name, sizeof(full_name), "%s.%s", state.info.mods[p->mod].name, p->name);
                    igPushIDStr(full_name);
                    ipSetNextPlotLimits(state.ps.min[mp], state.ps.max[mp], fg->min - 1.f, fg->max + 2.f, ImGuiCond_Once);
                    if (BeginPlotDefaults("##AA", full_name, "AA Rating")) {
                        for (i32 r = 0; r < NumSkillsets; r++) {
                            skillset_line_plot(r, ss_highlight[r], fg->absolute[r].xs, fg->absolute[r].ys, fg->len);
                        }
                        ipEndPlot();
                    }
                    igSameLine(0, 4);
                    ipSetNextPlotLimits(state.ps.min[mp], state.ps.max[mp], fg->relative_min - 0.05f, 1.05f, ImGuiCond_Once);
                    if (BeginPlotDefaults("##Relative AA", full_name, 0)) {
                        for (i32 r = 1; r < NumSkillsets; r++) {
                            skillset_line_plot(r, ss_highlight[r], fg->relative[r].xs, fg->relative[r].ys, fg->len);
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
    state.num_open_windows = num_open_windows;

    if (buf_len(state.files) == 0) {
        igSetNextWindowPos((ImVec2) { left_width + centre_width / 2.0f, ds.y / 2.f }, 0, (ImVec2) { 0.5f, 0.5f });

        igBegin("Drop", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        igText("Drop files, song folders or packs here");
        igEnd();
    }

    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();

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
    }

    if (param_toggled != -1) {
        if (state.parameter_graphs_enabled[param_toggled]) {
            buf_push(state.parameter_graph_order, param_toggled);
            for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                buf_push(sfi->graphs, make_parameter_graph(param_toggled));
            }
        } else {
            for (isize i = 0; i < buf_len(state.parameter_graph_order); i++) {
                if (state.parameter_graph_order[i] == param_toggled) {
                    buf_remove_sorted_index(state.parameter_graph_order, i);
                    break;
                }
            }

            for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
                for (isize i = 0; i != buf_len(sfi->graphs); i++) {
                    if (state.graphs[sfi->graphs[i]].param == param_toggled) {
                        free_parameter_graph(sfi->graphs[i]);
                        buf_remove_sorted_index(sfi->graphs, i);
                        break;
                    }
                }
            }
        }
    }

    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
        if (sfi->open) {
            i32 goal_index = state.update_index % 19;
            f32 goal = 0.82f + ((f32)goal_index / 100.f);
            ssxs[goal_index] = goal * 100.f;
            SkillsetRatings ssr;
            calc_go(&state.calc, &state.ps, sfi->notes, 1.0f, goal, &ssr);
            for (i32 r = 0; r < NumSkillsets; r++) {
                sfi->skillsets_over_wife[r][goal_index] = ssr.E[r];
                sfi->min_rating = min(ssr.E[r], sfi->min_rating);
                sfi->max_rating = max(ssr.E[r], sfi->max_rating);

                f32 relative_rating = ssr.E[r] / ssr.overall;
                sfi->relative_skillsets_over_wife[r][goal_index] = relative_rating;
                sfi->min_relative_rating = min(relative_rating, sfi->min_relative_rating);
                sfi->max_relative_rating = max(relative_rating, sfi->max_relative_rating);
            }

            for (isize i = 0; i != buf_len(sfi->graphs); i++) {
                FnGraph *fg = &state.graphs[sfi->graphs[i]];
                assert(fg->active);
                i32 x_index = state.update_index % 10;

                f32 current = state.ps.params[fg->param];
                state.ps.params[fg->param] = lerp(state.ps.min[fg->param], state.ps.max[fg->param], (f32)x_index / 9.0f);
                calc_go(&state.calc, &state.ps, sfi->notes, 1.0f, 0.93f, &ssr);
                state.ps.params[fg->param] = current;
                for (i32 r = 0; r < NumSkillsets; r++) {
                    for (isize x = 0; x < 10; x++) {
                        fg->absolute[r].xs[x] = lerp(state.ps.min[fg->param], state.ps.max[fg->param], (f32)x / 9.0f);
                        fg->relative[r].xs[x] = lerp(state.ps.min[fg->param], state.ps.max[fg->param], (f32)x / 9.0f);
                    }
                    fg->absolute[r].ys[x_index] = ssr.E[r];
                    fg->min = min(ssr.E[r], fg->min);
                    fg->max = max(ssr.E[r], fg->max);

                    f32 relative_rating = ssr.E[r] / ssr.overall;
                    fg->relative[r].ys[x_index] = relative_rating;
                    fg->relative_min = min(relative_rating, fg->relative_min);
                }
            }
        }
    }
    state.update_index += 127;
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
        .gl_force_gles2 = true,
        .window_title = "SeeMinaCalc",
        .ios_keyboard_resizes_canvas = false
    };
}
