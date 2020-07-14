#pragma warning(disable : 4116) // unnamed type definition in parentheses
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable : 4204) // nonstandard extension used: non-constant aggregate initializer
#pragma warning(disable : 4221) // nonstandard extension used: cannot be initialized using address of automatic variable
#pragma warning(disable : 4057) // 'initializing': 'char *' differs in indirection to slightly different base types from 'u8 *'

#include "sqlite3.c"

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

typedef struct SimFileWindow
{
    String title;
    String diff;
    String chartkey;

    NoteData *notes;
    EffectMasks effects;

    f32 *skillsets[NumSkillsetRatings];
    f32 *relative_skillsets[NumSkillsetRatings];
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

    CalcInfo ci;
    SimFileWindow sm;
} State;
static State state;
f32 xs[19];
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
    scratch = stack_make(malloc(bignumber), bignumber);
    permanent_memory = stack_make(malloc(bignumber), bignumber);

    CacheDB db = cachedb_init(db_path);
    Buffer cached = get_steps_from_db(&db, "X9a609c6dd132d807b2abc5882338cb9ebbec320d");
    // leak db

    state.ci = calc_init();
    state.sm.title = S("-Image-Material- (V0)");
    state.sm.diff = S("Challenge");
    state.sm.chartkey = S("X9a609c6dd132d807b2abc5882338cb9ebbec320d");
    state.sm.notes = frobble_serialized_note_data(cached.buf, cached.len);
    // state.sm.effects = calculate_effects(&state.ci, state.sm.notes);

    state.sm.min_rating = 40;
    state.sm.min_relative_rating = 2;
    for (i32 i = 0; i < 19; i++) {
        f32 goal = 0.82f + ((f32)i / 100.f);
        xs[i] = goal * 100.f;
        SkillsetRatings ssr;
        calc_go(state.ci.handle, state.sm.notes, 1.0f, goal, &ssr);
        for (i32 r = 0; r < NumSkillsetRatings; r++) {
            buf_push(state.sm.skillsets[r], ssr.E[r]);
            state.sm.min_rating = min(ssr.E[r], state.sm.min_rating);
            state.sm.max_rating = max(ssr.E[r], state.sm.max_rating);

            f32 relative_rating = ssr.E[r] / ssr.overall;
            buf_push(state.sm.relative_skillsets[r], relative_rating);
            state.sm.min_relative_rating = min(relative_rating, state.sm.min_relative_rating);
            state.sm.max_relative_rating = max(relative_rating, state.sm.max_relative_rating);
        }
    }
}

bool BeginPlotCppCpp(const char* title, const char* x_label, const char* y_label, const ImVec2* size, ImPlotFlags flags, ImPlotAxisFlags x_flags, ImPlotAxisFlags y_flags, ImPlotAxisFlags y2_flags, ImPlotAxisFlags y3_flags);

static bool ipBeginPlotDefaults(const char* title_id, const char* x_label, const char* y_label)
{
    return BeginPlotCppCpp(title_id, x_label, y_label, &(ImVec2){-1, 0}, ImPlotFlags_Default | ImPlotFlags_AntiAliased, ImPlotAxisFlags_Default, ImPlotAxisFlags_Default, ImPlotAxisFlags_Auxiliary, ImPlotAxisFlags_Auxiliary);
}

void frame(void)
{
    i32 width = sapp_width();
    i32 height = sapp_height();
    f64 delta_time = stm_sec(stm_laptime(&state.last_time));
    simgui_new_frame(width, height, delta_time);

    ipShowDemoWindow(0);

    ipPushStyleVarFloat(ImPlotStyleVar_LineWeight, 1.5f);
    igBegin(state.sm.title.buf, &state.sm.open, 0);
    {
        igText(state.sm.diff.buf);
        igSameLine(clamp_lowd(igGetWindowWidth() - 268, 100), 0);
        igText(state.sm.chartkey.buf);

        ipSetNextPlotLimits(82, 100, state.sm.min_rating - 1.f, state.sm.max_rating + 2.f, ImGuiCond_Once);
        if (ipBeginPlotDefaults("Rating", "%", "SSR")) {
            for (i32 r = 0; r < NumSkillsetRatings; r++) {
                ipPlotLineFloatPtrFloatPtr(SkillsetNames[r], xs, state.sm.skillsets[r], 19, 0, sizeof(float));
            }
            ipEndPlot();
        }

        ipSetNextPlotLimits(82, 100, state.sm.min_relative_rating - 0.05f, state.sm.max_relative_rating + 0.05f, ImGuiCond_Once);
        if (ipBeginPlotDefaults("Relative Rating", "%", "SSR / Overall")) {
            for (i32 r = 1; r < NumSkillsetRatings; r++) {
                ipPlotLineFloatPtrFloatPtr(SkillsetNames[r], xs, state.sm.relative_skillsets[r], 19, 0, sizeof(float));
            }
            ipEndPlot();
        }
    }
    igEnd();

    igShowDemoWindow(0);

    sg_begin_default_pass(&state.pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
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
        .gl_force_gles2 = true,
        .window_title = "SeeMinaCalc",
        .ios_keyboard_resizes_canvas = false
    };
}
