#include <cstring>
#include <string>
#include <cmath>
#include <memory>
#include <vector>
#include <string_view>
#include <cstddef>

#include "common.h"
#include "cminacalc.h"

using std::vector;

#if defined(NO_SSE)
// Replace fastsqrt with sqrt without modifying MinaCalc source
#define __SSE__
#include <xmmintrin.h>
#undef __SSE__
#define _mm_load_ss(f) *(f)
#define _mm_store_ss(out, val) (*(out) = (val))
#define _mm_mul_ss(a, b) sqrt(a)
#include "Etterna/MinaCalc/PatternModHelpers.h"
#undef _mm_load_ss
#undef _mm_store_ss
#undef _mm_mul_ss
#endif

#if defined(__EMSCRIPTEN__) && __cplusplus < 201703L
namespace std {
template<typename T>
T clamp(T t, T a, T b)
{
    return (t < a) ? a
         : (t > b) ? b
         : t;
}
}
#endif

// Building with clang and MSVC headers needs this.
#include <sstream>

#include "calcconstants.h"

// Changes made to the calc source:
//   - some constexpr globals changed to thread_local
//   - pointer to flat params array added to MinaCalc and distributed to ulbu by calling stupud_hack
//   - float literals are marked up with macros to make them accessible at runtime
//      - these are updated by writing into the same flat params array before calling the calc
//      - the calc needed to call stupud_hack because it used to re-init ulbu, and we change ulbu
//        params, but this doesn't happen anymore, so this could be redone
//   - axe taken to initadjdiff and guts sprewn about MinaCalc members
#include "Etterna/MinaCalc/MinaCalc.h"
#include "Etterna/MinaCalc/Ulbu.h"
static void stupud_hack(TheGreatBazoinkazoinkInTheSky *ulbu, float *mod_cursor);
#include "Etterna/MinaCalc/MinaCalc.cpp"

using ParamJunk = std::vector<std::pair<std::string,float*>>;
thread_local ParamJunk BaseScalers {
	// Overall not included
    { "Stream", (float *)(&basescalers[1]) },
    { "Jumpstream", (float *)&basescalers[2] },
    { "Handstream", (float *)&basescalers[3] },
    { "Stamina", (float *)&basescalers[4] },
    { "Jackspeed", (float *)&basescalers[5] },
    { "Chordjacks", (float *)&basescalers[6] },
    { "Technical", (float *)&basescalers[7] },
};

thread_local ParamJunk ManualConstants {
    { "MinaCalc.magic_num", &magic_num },
    { "MinaCalc.tech_pbm", &tech_pbm },
    { "MinaCalc.jack_pbm", &jack_pbm },
    { "MinaCalc.stream_pbm", &stream_pbm },
    { "MinaCalc.bad_newbie_skillsets_pbm", &bad_newbie_skillsets_pbm },
    { "SequencingHelpers.finalscaler", &finalscaler },
    { "WideRangeJumptrillMod.wrjt_cv_factor", &wrjt_cv_factor },
	{ "CJOHASequencing.chain_slowdown_scale_threshold ", &chain_slowdown_scale_threshold },
    { "GenericSequencing.anchor_spacing_buffer_ms", &anchor_spacing_buffer_ms },
    { "GenericSequencing.anchor_speed_increase_cutoff_factor", &anchor_speed_increase_cutoff_factor },
    { "GenericSequencing.jack_spacing_buffer_ms", &jack_spacing_buffer_ms },
    { "GenericSequencing.jack_speed_increase_cutoff_factor", &jack_speed_increase_cutoff_factor },
	{ "GenericSequencing.guaranteed_reset_buffer_ms", &guaranteed_reset_buffer_ms },
    { "RMSequencing.rma_diff_scaler", &rma_diff_scaler },
    { "SequenceBasedDiffCalc.scaler_for_ms_base", &scaler_for_ms_base },
    { "SequenceBasedDiffCalc.ms_base_finger_weighter_2", &ms_base_finger_weighter_2 },
    { "SequenceBasedDiffCalc.ms_base_finger_weighter", &ms_base_finger_weighter },
};

thread_local float anchor_len_cap_float = (float)anchor_len_cap;
thread_local float jack_len_cap_float = (float)jack_len_cap;

thread_local ParamJunk ManualConstantInts {
    { "GenericSequencing.anchor_len_cap", &anchor_len_cap_float },
    { "GenericSequencing.jack_len_cap", &jack_len_cap_float },
};

static void stupud_hack(TheGreatBazoinkazoinkInTheSky *ulbu, float *mod_cursor)
{
    for (const auto &p : ulbu->_s._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_js._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_hs._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_cj._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_cjd._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_hsd._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_ohj._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_cjohj._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_roll._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_bal._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_oht._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_voht._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_ch._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_chain._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_rm._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_mj._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_wrb._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_wrr._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_wrjt._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_wrjj._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_wra._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_fj._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_tt._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_tt2._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_diffz._tc._params) *p.second = *mod_cursor++;
    for (const auto &p : ulbu->_diffz._cj._params) *p.second = *mod_cursor++;
	for (const auto &p : BaseScalers) *p.second = *mod_cursor++;
	for (const auto &p : ManualConstantInts) *p.second = *mod_cursor++;
	for (const auto &p : ManualConstants) *p.second = *mod_cursor++;
	for (const auto &p : MinaCalcConstants) *p.second = *mod_cursor++;

    anchor_len_cap = (int)anchor_len_cap_float;
    jack_len_cap = (int)jack_len_cap_float;
}

static float absolute_value(float a)
{
    return (a >= 0) ? a : -a;
}

static float clamp_low(float a, float t)
{
    return (a > t) ? a : t;
}

static b32 str_eq(char const *a, char const *b)
{
    return strcmp(a, b) == 0;
}

struct {
    const char *name;
    const char *file;
    CalcPatternMod id;
} Mods[] = {
    { "Rate",                       0, CalcPatternMod_Invalid },
    { "Stream",                     "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/Stream.h", Stream },
    { "JS",                         "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/JS.h", JS },
    { "HS",                         "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/HS.h", HS },
    { "CJ",                         "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/CJ.h", CJ },
    { "CJDensity",                  "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/CJDensity.h", CJDensity },
    { "HSDensity",                  "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/HSDensity.h", HSDensity },
    { "OHJumpModGuyThing",          "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/OHJ.h", OHJumpMod },
    { "CJOHJump",                   "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/CJOHJ.h", CJOHJump },
    { "Roll",                       "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/Roll.h", Roll },
    { "Balance",                    "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/Balance.h", Balance },
    { "OHTrill",                    "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/OHT.h", OHTrill, },
    { "VOHTrill",                   "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/VOHT.h", VOHTrill },
    { "Chaos",                      "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/Chaos.h", Chaos },
    { "CJOHAnchor",                 "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/CJOHAnchor.h", CJOHAnchor },
    { "RunningMan",                 "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/RunningMan.h", RanMan },
    { "Minijack",                   "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/Minijack.h", Minijack },
    { "WideRangeBalance",           "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/WideRangeBalance.h", WideRangeBalance },
    { "WideRangeRoll",              "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/WideRangeRoll.h", WideRangeRoll },
    { "WideRangeJumptrill",         "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/WideRangeJumptrill.h", WideRangeJumptrill },
    { "WideRangeJJ",                "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/WideRangeJJ.h", WideRangeJJ },
    { "WideRangeAnchor",            "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/WideRangeAnchor.h", WideRangeAnchor },
    { "FlamJam",                    "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/FlamJam.h", FlamJam,  },
    { "TheThingLookerFinderThing",  "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/TheThingFinder.h", TheThing },
    { "TheThingLookerFinderThing2", "etterna/Etterna/MinaCalc/Agnostic/HA_PatternMods/TheThingFinder.h", TheThing2 },
    { "Techyo",                     "etterna/Etterna/MinaCalc/SequencedBaseDiffCalc.h", Techyo },
    { "Ceejay",                     "etterna/Etterna/MinaCalc/SequencedBaseDiffCalc.h", Ceejay },
    { "Base Scalers",               0, CalcPatternMod_Invalid },
    { "Global Integers",            0, CalcPatternMod_Invalid },
    { "Globals",                    0, CalcPatternMod_Invalid },
    { "Inline Constants",           0, CalcPatternMod_Invalid },
};
const char *BaseScalersFile = "etterna/Etterna/MinaCalc/UlbuAcolytes.h";

const char *GlobalFiles[] = {
    "etterna/Etterna/MinaCalc/MinaCalc.cpp",                                  // { "MinaCalc.magic_num", &magic_num },
    "etterna/Etterna/MinaCalc/MinaCalc.cpp",                                  // { "MinaCalc.tech_pbm", &tech_pbm },
    "etterna/Etterna/MinaCalc/MinaCalc.cpp",                                  // { "MinaCalc.jack_pbm", &jack_pbm },
    "etterna/Etterna/MinaCalc/MinaCalc.cpp",                                  // { "MinaCalc.stream_pbm", &stream_pbm },
    "etterna/Etterna/MinaCalc/MinaCalc.cpp",                                  // { "MinaCalc.bad_newbie_skillsets_pbm", &bad_newbie_skillsets_pbm },
    "etterna/Etterna/MinaCalc/SequencingHelpers.h",                           // { "SequencingHelpers.finalscaler", &finalscaler },
    "etterna/Etterna/MinaCalc/Dependent/HD_PatternMods/WideRangeJumptrill.h", // { "WideRangeJumptrillMod.wrjt_cv_factor", &wrjt_cv_factor },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/CJOHASequencing.h",     // { "CJOHASequencing.chain_slowdown_scale_threshold ", &chain_slowdown_scale_threshold },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/GenericSequencing.h",   // { "GenericSequencing.anchor_spacing_buffer_ms", &anchor_spacing_buffer_ms },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/GenericSequencing.h",   // { "GenericSequencing.anchor_speed_increase_cutoff_factor", &anchor_speed_increase_cutoff_factor },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/GenericSequencing.h",   // { "GenericSequencing.jack_spacing_buffer_ms", &jack_spacing_buffer_ms },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/GenericSequencing.h",   // { "GenericSequencing.jack_speed_increase_cutoff_factor", &jack_speed_increase_cutoff_factor },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/GenericSequencing.h",   // { "GenericSequencing.guaranteed_reset_buffer_ms", &guaranteed_reset_buffer_ms },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/RMSequencing.h",        // { "RMSequencing.rma_diff_scaler", &rma_diff_scaler },
    "etterna/Etterna/MinaCalc/SequencedBaseDiffCalc.h"                        // { "SequencedBaseDiffCalc.scaler_for_ms_base", &scaler_for_ms_base },
    "etterna/Etterna/MinaCalc/SequencedBaseDiffCalc.h"                        // { "SequencedBaseDiffCalc.ms_base_finger_weighter_2", &ms_base_finger_weighter_2 },
    "etterna/Etterna/MinaCalc/SequencedBaseDiffCalc.h"                        // { "SequencedBaseDiffCalc.ms_base_finger_weighter", &ms_base_finger_weighter },
};

const char *GlobalIntegerFiles[] = {
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/GenericSequencing.h",   // { "GenericSequencing.anchor_len_cap", &anchor_len_cap_float },
    "etterna/Etterna/MinaCalc/Dependent/HD_Sequencers/GenericSequencing.h",   // { "GenericSequencing.jack_len_cap", &jack_len_cap_float },
};

enum
{
    NumMods = sizeof(Mods) / sizeof(Mods[0])
};

struct DebugBuffers
{
    std::vector<std::vector<std::vector<float>>> handInfo[2];
    std::vector<float> row_time[2];
    std::vector<float> jack_diff[2];
    std::vector<float> jack_stam[2];
    std::vector<float> jack_loss[2];
    std::vector<float> interval_times;
};

struct NoteData
{
    const vector<NoteInfo> ref;
};

static float RateParam = 1.0f;
static const ParamJunk RateMod{{ "rate", (float *)&RateParam }};

NoteData *frobble_serialized_note_data(char const *note_data, size_t length)
{
    return new NoteData{{ (NoteInfo *)note_data, (NoteInfo *)(note_data + length) }};
}

NoteData *frobble_note_data(NoteInfo *note_data, size_t length)
{
    return frobble_serialized_note_data((char *)note_data, length * sizeof(NoteInfo));
}

isize note_data_row_count(NoteData *note_data)
{
    return (isize)note_data->ref.size();
}

NoteInfo const *note_data_rows(NoteData *note_data)
{
    return &note_data->ref[0];
}

void free_note_data(NoteData *note_data)
{
    delete note_data;
}

static const float BigNArbitrary = 100.5438f;
static float make_test_value(float default_value)
{
    if (default_value == 0.0f) {
        return BigNArbitrary;
    }
    return absolute_value(default_value) * BigNArbitrary;
}

CalcInfo calc_info()
{
    ModInfo *mod_info = (ModInfo *)calloc(NumMods, sizeof(ModInfo));

    // make_unique cause the calc is too big to fit on the stack :)
    auto dummy_calc = std::make_unique<Calc>();
    // static just so the param strings aren't freed :)
    static auto shalhoub = TheGreatBazoinkazoinkInTheSky(*dummy_calc);

    // Order here must match the order of the global Mods array
    const std::vector<std::pair<std::string,float*>> *params[NumMods] = {
        &RateMod,
        &shalhoub._s._params,
        &shalhoub._js._params,
        &shalhoub._hs._params,
        &shalhoub._cj._params,
        &shalhoub._cjd._params,
        &shalhoub._hsd._params,
        &shalhoub._ohj._params,
        &shalhoub._cjohj._params,
        &shalhoub._roll._params,
        &shalhoub._bal._params,
        &shalhoub._oht._params,
        &shalhoub._voht._params,
        &shalhoub._ch._params,
        &shalhoub._chain._params,
        &shalhoub._rm._params,
        &shalhoub._mj._params,
        &shalhoub._wrb._params,
        &shalhoub._wrr._params,
        &shalhoub._wrjt._params,
        &shalhoub._wrjj._params,
        &shalhoub._wra._params,
        &shalhoub._fj._params,
        &shalhoub._tt._params,
        &shalhoub._tt2._params,
        &shalhoub._diffz._tc._params,
        &shalhoub._diffz._cj._params,
        &BaseScalers,
        &ManualConstantInts,
        &ManualConstants,
        &MinaCalcConstants,
    };

    int num_params = 0;
    for (isize i = 0; i < NumMods; i++) {
        mod_info[i].name = Mods[i].name;
        mod_info[i].num_params = (int)params[i]->size();
        mod_info[i].index = num_params;
        num_params += mod_info[i].num_params;
    }

    ParamInfo *param_info = (ParamInfo *)calloc(num_params, sizeof(ParamInfo));
    ParamInfo *param_info_cursor = param_info;

    std::vector<float *> param_pointers;
    param_pointers.reserve(num_params);

    b32 is_constant = false;
    for (isize i = 0; i < NumMods; i++) {
        if (params[i] == &BaseScalers) {
            // Every param after this iteration is a literal constant in the code somewhere
            is_constant = true;
        }
        for (const auto& p : *params[i]) {
            param_info_cursor->name = p.first.c_str();
            param_info_cursor->mod = (int)i;
            param_info_cursor->default_value = *p.second;
            param_info_cursor->constant = is_constant;
            param_info_cursor->optimizable = true;
            param_info_cursor++;

            param_pointers.push_back(p.second);
        }
    }

    // Test for clamping high
    for (isize i = 0; i < num_params; i++) {
        *param_pointers[i] = make_test_value(param_info[i].default_value);
    }

    shalhoub.setup_agnostic_pmods();
    shalhoub.setup_dependent_mods();

    for (isize i = 0; i < num_params; i++) {
        if (str_eq((char *)param_info[i].name, "window_param")
        // This is getting silly
        || str_eq((char *)param_info[i].name, "intervals_to_consider")
        || str_eq((char *)param_info[i].name, "chaos_comp_window")
        || str_eq((char *)param_info[i].name, "tc_static_base_window")) {
            param_info[i].integer = true;
            param_info[i].optimizable = false;
            param_info[i].min = 0;
            param_info[i].max = max_moving_window_size - 1;
        } else if (str_eq((char *)param_info[i].name, "prop_buffer")) {
            param_info[i].max = nextafter(2.0f, 0.0f);
        } else {
            float test_value = make_test_value(param_info[i].default_value);
            if (test_value != *param_pointers[i]) {
                param_info[i].max = *param_pointers[i];
            } else {
                float a = absolute_value(param_info[i].default_value);
                if (a < 0.5f) {
                    param_info[i].max = 1.0f;
                } else {
                    param_info[i].max = absolute_value(param_info[i].default_value) * 2.0f;
                }
            }
        }
    }

    // Test for clamping low
    for (isize i = 0; i < num_params; i++) {
        *param_pointers[i] = -1.0f * make_test_value(param_info[i].default_value);
    }

    shalhoub.setup_agnostic_pmods();
    shalhoub.setup_dependent_mods();

    for (isize i = 0; i < num_params; i++) {
        if (param_info[i].constant &&
            (  str_eq((char *)param_info[i].name, "MinaCalc.cpp(73, 2)")
            || str_eq((char *)param_info[i].name, "MinaCalc.cpp(91)")
            || str_eq((char *)param_info[i].name, "MinaCalc.cpp(182, 2)")
            || str_eq((char *)param_info[i].name, "MinaCalc.cpp(182, 4)"))) {
            // Hack to fix bad bad no good infinite loop causer
            // chisel P(10.24) and P(0.32). should add P_MIN(10.24, 0.1) or something
            param_info[i].min = 0.1f;
        } else if (param_info[i].integer == false) {
            float test_value = -1.0f * make_test_value(param_info[i].default_value);
            if (test_value != *param_pointers[i]) {
                param_info[i].min = *param_pointers[i];
            } else {
                // Try to guess parameters that make sense to go below zero
                float a = absolute_value(param_info[i].default_value);
                if (a < 0.5f && param_info[i].constant == false) {
                    param_info[i].min = -1.0f;
                } else if (a <= 2.f) {
                    param_info[i].min = 0.0f;
                } else {
                    param_info[i].min = 0.1f;
                }
            }
        }
    }

    // Special case for rate
    param_info[0].min = 0.5;
    param_info[0].max = 3.0;
    param_info[0].optimizable = false;

    // Special case for constant ints
    for (isize i = 0; i < num_params; i++) {
        if (str_eq(mod_info[param_info[i].mod].name, "Global Integers")) {
            param_info[i].min = 0;
            param_info[i].max = 10;
            param_info[i].optimizable = false;
            param_info[i].integer = true;
        }
    }

    size_t pmods_used_count = NUM_CalcPatternMod * NUM_Skillset;
    bool *pmods_used = (bool *)calloc(pmods_used_count, sizeof(bool));
    size_t pmod_used_index = 0;
    size_t ss_index = 0;
    for (auto& ss : dummy_calc->pmods_used) {
        for (auto pmod : ss) {
            pmods_used[(pmod * NUM_Skillset) + ss_index] = true;
        }
        ss_index++;
    }

    size_t op_count = 0;
    for (auto& ss : dummy_calc->adj_diff_ops) {
        for (AdjDiff& op : ss) {
            op_count++;
        }
    }

    AdjDiff *ops = (AdjDiff *)calloc(op_count, sizeof(AdjDiff));
    size_t op_index = 0;
    ss_index = 0;
    for (auto& ss : dummy_calc->adj_diff_ops) {
        for (AdjDiff& op : ss) {
            ops[op_index] = op;
            ops[op_index].ss = (Skillset)ss_index;
            ops[op_index].enabled = true;
            op_index++;
        }
        ss_index++;
    }

    ParamSet defaults = {};
    defaults.params = (float *)calloc(num_params, sizeof(float));
    defaults.min = (float *)calloc(num_params, sizeof(float));
    defaults.max = (float *)calloc(num_params, sizeof(float));
    defaults.num_params = num_params;
    defaults.pmods_used = pmods_used;
    defaults.num_pmods_used = pmods_used_count;
    defaults.ops = ops;
    defaults.num_ops = op_count;
    for (size_t i = 0; i < num_params; i++) {
        defaults.params[i] = param_info[i].default_value;
        defaults.min[i] = param_info[i].min;
        defaults.max[i] = param_info[i].max;
    }
    for (size_t i = 0; i < NumSkillsets; i++) {
        defaults.adj_diff_base[i] = dummy_calc->adj_diff_bases[i];
    }

    CalcInfo result = {};
    result.version = GetCalcVersion();
    result.num_mods = NumMods;
    result.num_params = num_params;
    result.mods = mod_info;
    result.params = param_info;
    result.defaults = defaults;

    assert(str_eq(mod_info[NumMods - 1].name, "Inline Constants"));
    ModInfo *inlines_mod = &mod_info[NumMods - 1];
    for (size_t i = 0; i < inlines_mod->num_params; i++) {
        size_t p = inlines_mod->index + i;
        InlineConstantInfo *icf =  info_for_inline_constant(&result, p);
        param_info[p].optimizable = icf->optimizable;
        if (icf->optimizable == false) {
            param_info[p].fake = true;
        }

        assert_implies(param_info[p].fake, param_info[p].optimizable == false);
        assert_implies(param_info[p].integer, param_info[p].optimizable == false);
        assert_implies(param_info[p].optimizable, !param_info[p].fake && !param_info[p].integer);
    }

    return result;
}

const char *file_for_param(CalcInfo *info, size_t param_index)
{
    const char *result = 0;

    assert(param_index >= 0 && param_index < info->num_params);
    ParamInfo *p = &info->params[param_index];
    if (Mods[p->mod].file) {
        result = Mods[p->mod].file;
    } else {
        ModInfo *m = &info->mods[p->mod];
        if (str_eq(m->name, "Globals")) {
            ModInfo *m = &info->mods[p->mod];
            size_t idx = param_index - m->index;
            assert(idx < array_length(GlobalFiles));
            result = GlobalFiles[idx];
        } else if (str_eq(m->name, "Global Integers")) {
            ModInfo *m = &info->mods[p->mod];
            size_t idx = param_index - m->index;
            assert(idx < array_length(GlobalIntegerFiles));
            result = GlobalIntegerFiles[idx];
        } else if (str_eq(m->name, "Base Scalers")) {
            return BaseScalersFile;
        } else {
            InlineConstantInfo *icf = info_for_inline_constant(info, param_index);
            return icf ? icf->file : 0;
        }
    }

    return result;
}

InlineConstantInfo *info_for_inline_constant(CalcInfo *info, size_t param_index)
{
    assert(param_index >= 0 && param_index < info->num_params);
    ParamInfo *p = &info->params[param_index];
    ModInfo *m = &info->mods[p->mod];
    assert(str_eq(m->name, "Inline Constants"));
    return &where_u_at[param_index - m->index];
}

void copy_param_set_to_calc(Calc *c, ParamSet *params)
{
    memcpy(c->mod_params, params->params, params->num_params * sizeof(float));
    memcpy(c->adj_diff_bases.data(), params->adj_diff_base, NumSkillsets * sizeof(CalcDiffValue));
    for (ptrdiff_t ss = 1; ss < NUM_Skillset; ss++) {
        c->pmods_used[ss].clear();
        for (ptrdiff_t mod = 0; mod < NUM_CalcPatternMod; mod++) {
            if (params->pmods_used[(mod * NUM_Skillset) + ss]) {
                c->pmods_used[ss].push_back(mod);
            }
        }
    }
    for (ptrdiff_t i = 0; i < NumSkillsets; i++) {
        c->adj_diff_ops[i].clear();
    }
    for (ptrdiff_t i = 0; i < params->num_ops; i++) {
        if (params->ops[i].enabled) {
            c->adj_diff_ops[params->ops[i].ss].push_back(params->ops[i]);
        }
    }
}

auto
CMinaCalc_MinaSDCalc(const std::vector<NoteInfo>& note_info,
		   float musicrate,
		   float goal,
		   Calc* calc) -> std::vector<float>
{
	if (note_info.size() <= 1) {
		return dimples_the_all_zero_output;
	}
    goal = std::clamp(goal, 0.8f, 0.965f);
	calc->ssr = (goal != 0.93f);
    auto result = calc->CalcMain(note_info, musicrate, min(goal, ssr_goal_cap));
	calc->debugmode = false;
    return result;
}

SeeCalc calc_init(CalcInfo *info)
{
    SeeCalc result = {};
    result.handle = new Calc();
    result.handle->mod_params = (float *)calloc(info->num_params, sizeof(float));
    for (size_t i = 0; i < info->num_params; i++) {
        result.handle->mod_params[i] = info->params[i].default_value;
    }
    return result;
}

SkillsetRatings calc_go(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal)
{
    SkillsetRatings result = {};
    copy_param_set_to_calc(calc->handle, params);
    vector<float> ratings = CMinaCalc_MinaSDCalc(note_data->ref, clamp_low(1e-5f, params->params[0]), goal, calc->handle);
    for (int i = 0; i < NumSkillsets; i++) {
        result.E[i] = ratings[i];
    }
    return result;
}

SkillsetRatings calc_go_with_param(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal, int param, float value)
{
    SkillsetRatings result = {};
    copy_param_set_to_calc(calc->handle, params);
    calc->handle->mod_params[param] = value;
    vector<float> ratings = CMinaCalc_MinaSDCalc(note_data->ref, clamp_low(1e-5f, param == 0 ? value : params->params[0]), goal, calc->handle);
    for (int i = 0; i < NumSkillsets; i++) {
        result.E[i] = ratings[i];
    }
    return result;
}

SkillsetRatings calc_go_with_rate_and_param(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal, float rate, int param, float value)
{
    SkillsetRatings result = {};
    copy_param_set_to_calc(calc->handle, params);
    calc->handle->mod_params[param] = value;
    vector<float> ratings = CMinaCalc_MinaSDCalc(note_data->ref, clamp_low(1e-5f, rate), goal, calc->handle);
    for (int i = 0; i < NumSkillsets; i++) {
        result.E[i] = ratings[i];
    }
    return result;
}

DebugInfo calc_go_debuginfo(SeeCalc *calc, ParamSet *params, NoteData *note_data, float rate)
{
    DebugInfo result = {};
    result.buffers = new DebugBuffers;
    calc->handle->debugmode = true;
    copy_param_set_to_calc(calc->handle, params);
    CMinaCalc_MinaSDCalc(note_data->ref, clamp_low(1e-5f, rate), 0.93f, calc->handle);

    for (ptrdiff_t h = 0; h < 2; h++) {
        result.buffers->handInfo[h] = std::move(calc->handle->debugValues[h]);

        size_t length = result.buffers->handInfo[h][0][0].size();
        for (ptrdiff_t i = 0; i < NUM_CalcPatternMod; i++) {
            result.interval_hand[h].pmod[i] = result.buffers->handInfo[h][0][i].data();
            assert(length == result.buffers->handInfo[h][0][i].size());

            if (h == 0) {
                for (auto& v : result.buffers->handInfo[h][0][i]) {
                    v = -v;
                }
            }
        }
        for (ptrdiff_t i = 0; i < NUM_CalcDiffValue; i++) {
            result.interval_hand[h].diff[i] = result.buffers->handInfo[h][1][i].data();
            assert(length == result.buffers->handInfo[h][1][i].size());

            if (h == 0) {
                for (auto& v : result.buffers->handInfo[h][1][i]) {
                    v = -v;
                }
            }
        }
        for (ptrdiff_t i = 0; i < NUM_CalcDebugMisc; i++) {
            result.interval_hand[h].misc[i] = result.buffers->handInfo[h][2][i].data();
            assert(length == result.buffers->handInfo[h][2][i].size());

            if (h == 0) {
                for (auto& v : result.buffers->handInfo[h][2][i]) {
                    v = -v;
                }
            }
        }

        result.interval_hand[h].length = ptrdiff_t(length);

        result.buffers->row_time[h].resize(calc->handle->jack_diff[h].size());
        result.buffers->jack_diff[h].resize(calc->handle->jack_diff[h].size());
        result.buffers->jack_stam[h].resize(calc->handle->jack_diff[h].size());
        result.buffers->jack_loss[h].resize(calc->handle->jack_diff[h].size());
        f32 left_hand_negate = (h == 0) ? -1.0f : 1.0f;
        for (ptrdiff_t i = 0; i < calc->handle->jack_diff[h].size(); i++) {
            result.buffers->row_time[h][i] = calc->handle->jack_diff[h][i].first;
            result.buffers->jack_diff[h][i] = left_hand_negate * calc->handle->jack_diff[h][i].second;
            result.buffers->jack_stam[h][i] = left_hand_negate * ((i < calc->handle->jack_stam_stuff[h].size()) ? calc->handle->jack_stam_stuff[h][i] : 0.0f);
            result.buffers->jack_loss[h][i] = left_hand_negate * ((i < calc->handle->jack_loss[h].size())       ? calc->handle->jack_loss[h][i] : 0.0f);
        }

        result.jack_hand[h].row_time  = result.buffers->row_time[h].data();
        result.jack_hand[h].jack_diff = result.buffers->jack_diff[h].data();
        result.jack_hand[h].jack_stam = result.buffers->jack_stam[h].data();
        result.jack_hand[h].jack_loss = result.buffers->jack_loss[h].data();
        result.jack_hand[h].length = ptrdiff_t(result.buffers->row_time[h].size());

        result.buffers->interval_times.resize(length);
        for (ptrdiff_t i = 0; i < length; i++) {
            result.buffers->interval_times[i] = float(i) * 0.5f;
        }
        result.interval_times = result.buffers->interval_times.data();
        result.n_intervals = result.buffers->interval_times.size();
    }
    assert(result.interval_hand[0].length == result.interval_hand[1].length);
    return result;
}

void debuginfo_free(DebugInfo *info)
{
    *info->buffers = {};
    delete info->buffers;
    info->buffers = 0;
}

size_t debuginfo_mod_index(size_t mod)
{
    if (mod >= NumMods) {
        return (size_t)CalcPatternMod_Invalid;
    }
    return (size_t)Mods[mod].id;
}
