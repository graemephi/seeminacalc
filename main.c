#pragma warning(disable : 4116) // unnamed type definition in parentheses
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable : 4204) // nonstandard extension used: non-constant aggregate initializer
#pragma warning(disable : 4221) // nonstandard extension used: cannot be initialized using address of automatic variable
#pragma warning(disable : 4057) // 'initializing': 'char *' differs in indirection to slightly different base types from 'u8 *'

#include "sqlite3.c"
// The perils of including sqlite3 in your translation unit
#if defined(_DEBUG) && NDEBUG == 1
#undef NDEBUG
#endif

#define SOKOL_GLCORE33
#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "cimgui/cimplot.h"
#define SOKOL_IMGUI_IMPL
#include "sokol/util/sokol_imgui.h"

#include "bottom.h"
#include "cminacalc.h"
#include "sm.h"

#pragma float_control(precise, on, push)
#include "sm.c"
#pragma float_control(pop)

typedef struct CacheDB
{
    sqlite3 *db;
    sqlite3_stmt *note_data_stmt;
} CacheDB;

static const char *db_path = 0;

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

typedef struct SimFileWindow
{
    String title;
    String diff;
    String chartkey;

    NoteData *notes;
    EffectMasks effects;

    f32 *skillsets_over_wife[NumSkillsetRatings];
    f32 *relative_skillsets_over_wife[NumSkillsetRatings];
    f32 min_rating;
    f32 max_rating;
    f32 min_relative_rating;
    f32 max_relative_rating;

    bool open;
} SimFileWindow;

typedef struct State
{
    u64 last_time;
    sg_pass_action pass_action;

    CalcInfo info;
    SeeCalc calc;
    SimFileWindow sm;
    ParamSet ps;

    bool selected_skillsets[NumSkillsetRatings];
    u32 skillset_colors[NumSkillsetRatings];
    u32 skillset_colors_selectable[NumSkillsetRatings];

    int update_index;
} State;
static State state;

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
float ssxs[19];
void init(void)
{
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    stm_setup();
    simgui_setup(&(simgui_desc_t){ .no_default_font = 1 });
    state = (State) {
        .pass_action = {
            .colors[0] = {
                .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.0f, 0.0f, 1.0f }
            }
        }
    };

    ImGuiIO* io = igGetIO();
    ImFontAtlas_AddFontFromFileTTF(io->Fonts, "NotoSansCJKjp-Regular.otf", 16.0f, 0, ImFontAtlas_GetGlyphRangesJapanese(io->Fonts));

    {
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

    isize bignumber = 100*1024*1024;
    scratch_stack = stack_make(malloc(bignumber), bignumber);
    permanent_memory_stack = stack_make(malloc(bignumber), bignumber);

    push_allocator(permanent_memory);

    Buffer f = read_file("./The Lost Dedicated Life.sm");
    SmFile sm = parse_sm(f);
    String ck = generate_chart_key(&sm, 0); ck;

    CacheDB db = cachedb_init(db_path);
    Buffer cached = get_steps_from_db(&db, ck.buf);
    // leak db

    state.info = calc_info();
    state.calc = calc_init(&state.info);
    state.sm.title = S("-Image-Material- (V0)");
    state.sm.diff = S("Challenge");
    state.sm.chartkey = S("X9a609c6dd132d807b2abc5882338cb9ebbec320d");
    state.sm.notes = frobble_serialized_note_data(cached.buf, cached.len);
    buf_reserve(state.sm.effects.weak, state.info.num_params);
    buf_reserve(state.sm.effects.strong, state.info.num_params);
    // calculate_effects(&state.info, &state.calc, state.sm.notes, &state.sm.effects);

    NoteInfo *ni = sm_to_ett_note_info(&sm, 0);
    NoteData *notes2 = frobble_note_data(ni, buf_len(ni));
    SkillsetRatings ssr1;
    calc_go(&state.calc, &state.info.defaults, state.sm.notes, 1.0f, .93f, &ssr1);
    SkillsetRatings ssr2;
    calc_go(&state.calc, &state.info.defaults, notes2, 1.0f, .93f, &ssr2);
    state.ps = copy_param_set(&state.info.defaults);
    nddump(state.sm.notes, notes2);

    state.sm.min_rating = 40;
    state.sm.min_relative_rating = 2;
    for (i32 i = 0; i < 19; i++) {
        f32 goal = 0.82f + ((f32)i / 100.f);
        ssxs[i] = goal * 100.f;
        SkillsetRatings ssr;
        calc_go(&state.calc, &state.info.defaults, state.sm.notes, 1.0f, goal, &ssr);
        for (i32 r = 0; r < NumSkillsetRatings; r++) {
            buf_push(state.sm.skillsets_over_wife[r], ssr.E[r]);
            state.sm.min_rating = min(ssr.E[r], state.sm.min_rating);
            state.sm.max_rating = max(ssr.E[r], state.sm.max_rating);

            f32 relative_rating = ssr.E[r] / ssr.overall;
            buf_push(state.sm.relative_skillsets_over_wife[r], relative_rating);
            state.sm.min_relative_rating = min(relative_rating, state.sm.min_relative_rating);
            state.sm.max_relative_rating = max(relative_rating, state.sm.max_relative_rating);
        }
    }

    for (isize i = 1; i < NumSkillsetRatings; i++) {
        state.selected_skillsets[i] = (0.9 <= (state.sm.skillsets_over_wife[i][18] / state.sm.skillsets_over_wife[0][18]));
    }
    for (isize i = 0; i < NumSkillsetRatings; i++) {
        ImVec4 c = GetColormapColor(i);
        state.skillset_colors[i] = igColorConvertFloat4ToU32(c);
        c.w *= 0.75;
        state.skillset_colors_selectable[i] = igColorConvertFloat4ToU32(c);
    }

    pop_allocator();
}

static void param_slider_widget(i32 param_idx)
{
    i32 mp = param_idx;
    igPushIDInt(mp);
    if (igButton("      ##reset", (ImVec2) {0})) {
        state.ps.params[mp] = state.info.defaults.params[mp];
        state.ps.min[mp] = state.info.defaults.min[mp];
        state.ps.max[mp] = state.info.defaults.max[mp];
    }
    igSameLine(0, 4);
    if (state.info.params[mp].integer) {
        i32 value = (i32)state.ps.params[mp];
        i32 low = (i32)state.ps.min[mp];
        i32 high = (i32)state.ps.max[mp];
        igSliderInt(state.info.params[mp].name, &value, low, high, "%d");
        state.ps.params[mp] = (f32)value;
    } else {
        f32 speed = (state.ps.max[mp] - state.ps.min[mp]) / 100.f;
        char slider_id[32];
        snprintf(slider_id, sizeof(slider_id), "##slider%d", mp);
        igSliderFloat(slider_id, &state.ps.params[mp], state.ps.min[mp], state.ps.max[mp], "%f", 1.0f);
        if (ItemDoubleClicked(0)) {
            state.ps.params[mp] = state.info.defaults.params[mp];
        }
        igSameLine(0, 4);
        igPushItemWidth(igGetFontSize() * 4.0f);
        igDragFloatRange2("##range",  &state.ps.min[mp], &state.ps.max[mp], speed, -100.0f, 100.0f, "%.1f", "%.1f", 1.0f);
        igPopItemWidth();
        igSameLine(0, 0);
        igText(state.info.params[mp].name);
    }
    igPopID();
}

static void skillset_line_plot(i32 ss, b32 highlight, f32 *xs, f32 *ys, i32 count)
{
    // Recreate highlighting cause ImPlot doesn't let you tell it to highlight lines
    if (highlight) {
        ipPushStyleVarFloat(ImPlotStyleVar_LineWeight,  ipGetStyle()->LineWeight * 2.0f);
    }

    ipPushStyleColorU32(ImPlotCol_Line, state.skillset_colors[ss]);
    ipPlotLineFloatPtrFloatPtr(SkillsetNames[ss], xs, ys, count, 0, sizeof(float));
    ipPopStyleColor(1);

    if (highlight) {
        ipPopStyleVar(1);
    }
}

void frame(void)
{
    i32 width = sapp_width();
    i32 height = sapp_height();
    f64 delta_time = stm_sec(stm_laptime(&state.last_time));
    simgui_new_frame(width, height, delta_time);

    igShowDemoWindow(0);

    bool ss_highlight[NumSkillsetRatings] = {0};

    igSetNextWindowSize((ImVec2) { 600, 0 }, ImGuiCond_Once);
    if (igBegin("Mod Parameters", 0, ImGuiWindowFlags_None)) {
        u32 mask = 0;
        i32 clear_selections_to = -1;
        for (isize i = 0; i < NumSkillsetRatings; i++) {
            igPushStyleColorU32(ImGuiCol_Header, state.skillset_colors_selectable[i]);
            igPushStyleColorU32(ImGuiCol_HeaderHovered, state.skillset_colors[i]);
            igSelectableBoolPtr(SkillsetNames[i], &state.selected_skillsets[i], 0, (ImVec2){ 500.0f / NumSkillsetRatings, 0});
            igPopStyleColor(2);
            if (ItemDoubleClicked(0)) {
                clear_selections_to = i;
            }
            if (igIsItemHovered(0)) {
                ss_highlight[i] = 1;
            }
            igSameLine(0, 4);
            igDummy((ImVec2){4,0});
            igSameLine(0, 4);
            mask |= (state.selected_skillsets[i] << i);
        }
        if (clear_selections_to >= 0) {
            memset(state.selected_skillsets, 0, sizeof(state.selected_skillsets));
            state.selected_skillsets[clear_selections_to] = 1;
        }
        igNewLine();

        if (igBeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
        {
            if (igBeginTabItem("More Relevant", 0, ImGuiTabItemFlags_None))
            {
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if ((state.sm.effects.strong[mp] & mask) != 0) {
                                param_slider_widget(mp);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            }
            if (igBeginTabItem("Less Relevant", 0, ImGuiTabItemFlags_None))
            {
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            if ((state.sm.effects.weak[mp] & mask) != 0) {
                                param_slider_widget(mp);
                            }
                        }
                        igTreePop();
                    }
                }
                igEndTabItem();
            }
            if (igBeginTabItem("All", 0, ImGuiTabItemFlags_None))
            {
                for (i32 i = 0; i < state.info.num_mods; i++) {
                    if (igTreeNodeExStr(state.info.mods[i].name, ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (i32 j = 0; j < state.info.mods[i].num_params; j++) {
                            i32 mp = state.info.mods[i].index + j;
                            param_slider_widget(mp);
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

    if (igBegin(state.sm.title.buf, &state.sm.open, ImGuiWindowFlags_None)) {
        igText(state.sm.diff.buf);
        igSameLine(clamp_lowd(igGetWindowWidth() - 268, 100), 0);
        igText(state.sm.chartkey.buf);

        ipSetNextPlotLimits(82, 100, state.sm.min_rating - 1.f, state.sm.max_rating + 2.f, ImGuiCond_Once);
        if (BeginPlotDefaults("Rating", "Wife%", "SSR")) {
            for (i32 r = 0; r < NumSkillsetRatings; r++) {
                skillset_line_plot(r, ss_highlight[r], ssxs, state.sm.skillsets_over_wife[r], 19);
            }
            ipEndPlot();
        }
        igSameLine(0, 4);
        ipSetNextPlotLimits(82, 100, state.sm.min_relative_rating - 0.05f, state.sm.max_relative_rating + 0.05f, ImGuiCond_Once);
        if (BeginPlotDefaults("Relative Rating", "Wife%", 0)) {
            for (i32 r = 1; r < NumSkillsetRatings; r++) {
                skillset_line_plot(r, ss_highlight[r], ssxs, state.sm.relative_skillsets_over_wife[r], 19);
            }
            ipEndPlot();
        }
    }
    igEnd();

    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();

    for (i32 i = state.update_index; i < state.update_index + 1; i++) {
        f32 goal = 0.82f + ((f32)i / 100.f);
        ssxs[i] = goal * 100.f;
        SkillsetRatings ssr;
        calc_go(&state.calc, &state.ps, state.sm.notes, 1.0f, goal, &ssr);
        for (i32 r = 0; r < NumSkillsetRatings; r++) {
            state.sm.skillsets_over_wife[r][i] = ssr.E[r];
            state.sm.min_rating = min(ssr.E[r], state.sm.min_rating);
            state.sm.max_rating = max(ssr.E[r], state.sm.max_rating);

            f32 relative_rating = ssr.E[r] / ssr.overall;
            state.sm.relative_skillsets_over_wife[r][i] = relative_rating;
            state.sm.min_relative_rating = min(relative_rating, state.sm.min_relative_rating);
            state.sm.max_relative_rating = max(relative_rating, state.sm.max_relative_rating);
        }
    }
    state.update_index += 1;
    if (state.update_index >= 19) {
        state.update_index = 0;
    }
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
    if (argc == 1) {
        printf("gimme a db\n");
        exit(1);
    }

    db_path = argv[1];

    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 1024,
        .height = 768,
        .sample_count = 8,
        .gl_force_gles2 = true,
        .window_title = "SeeMinaCalc",
        .ios_keyboard_resizes_canvas = false
    };
}
