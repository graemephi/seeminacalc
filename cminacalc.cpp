
#include <memory>
#include <vector>
#include <string_view>
#include <cstddef>

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

// Note on updating the calc: not quite drag-and-drop. grep for "Stupud hack"
#include "Etterna/MinaCalc/MinaCalc.h"
#include "Etterna/MinaCalc/MinaCalc.cpp"


#include "common.h"
#include "cminacalc.h"

static f32 absolute_value(f32 a)
{
    return (a >= 0) ? a : -a;
}

static f32 clamp_low(f32 a, f32 t)
{
    return (a > t) ? a : t;
}

const char *ModNames[] = {
    "Rate",
    "StreamMod",
    "JSMod",
    "HSMod",
    "CJMod",
    "CJDensityMod",
    "OHJumpModGuyThing",
    "CJOHJumpMod",
    "RollMod",
    "BalanceMod",
    "OHTrillMod",
    "VOHTrillMod",
    "ChaosMod",
    "RunningManMod",
    "WideRangeBalanceMod",
    "WideRangeRollMod",
    "WideRangeJumptrillMod",
    "WideRangeAnchorMod",
    "FlamJamMod",
    "TheThingLookerFinderThing",
    "TheThingLookerFinderThing2",
};

enum
{
    NumMods = sizeof(ModNames) / sizeof(char *)
};

struct NoteData
{
    const vector<NoteInfo> ref;
};

static float RateParam = 1.0f;
static const std::vector<std::pair<std::string,float*>> RateMod{{ "rate", (float *)&RateParam }};

NoteData *frobble_serialized_note_data(char *note_data, size_t length)
{
    return new NoteData{{ (NoteInfo *)note_data, (NoteInfo *)(note_data + length) }};
}

NoteData *frobble_note_data(NoteInfo *note_data, size_t length)
{
    return frobble_serialized_note_data((char *)note_data, length * sizeof(NoteInfo));
}

static const f32 BigNArbitrary = 100.5438f;
static f32 make_test_value(f32 default_value)
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

    const std::vector<std::pair<std::string,float*>> *params[NumMods] = {
        &RateMod,
        &shalhoub._s._params,
        &shalhoub._js._params,
        &shalhoub._hs._params,
        &shalhoub._cj._params,
        &shalhoub._cjd._params,
        &shalhoub._ohj._params,
        &shalhoub._cjohj._params,
        &shalhoub._roll._params,
        &shalhoub._bal._params,
        &shalhoub._oht._params,
        &shalhoub._voht._params,
        &shalhoub._ch._params,
        &shalhoub._rm._params,
        &shalhoub._wrb._params,
        &shalhoub._wrr._params,
        &shalhoub._wrjt._params,
        &shalhoub._wra._params,
        &shalhoub._fj._params,
        &shalhoub._tt._params,
        &shalhoub._tt2._params,
    };

    i32 last_param_start_index = 0;
    i32 num_params = 0;
    for (isize i = 0; i < NumMods; i++) {
        mod_info[i].name = ModNames[i];
        mod_info[i].num_params = (i32)params[i]->size();
        mod_info[i].index = num_params;
        last_param_start_index = num_params;
        num_params += mod_info[i].num_params;
    }

    ParamInfo *param_info = (ParamInfo *)calloc(num_params, sizeof(ParamInfo));
    ParamInfo *param_info_cursor = param_info;

    std::vector<f32 *> param_pointers;
    param_pointers.reserve(num_params);

    for (isize i = 0; i < NumMods; i++) {
        for (const auto& p : *params[i]) {
            param_info_cursor->name = p.first.c_str();
            param_info_cursor->mod = (i32)i;
            param_info_cursor->default_value = *p.second;
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
        if (strcmp((char *)param_info[i].name, "window_param") == 0) {
            param_info[i].integer = true;
            param_info[i].min = 0;
            param_info[i].max = max_moving_window_size - 1;
        } else {
            f32 test_value = make_test_value(param_info[i].default_value);
            if (test_value != *param_pointers[i]) {
                param_info[i].max = *param_pointers[i];
            } else {
                f32 a = absolute_value(param_info[i].default_value);
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
        if (param_info[i].integer == false) {
            f32 test_value = -1.0f * make_test_value(param_info[i].default_value);
            if (test_value != *param_pointers[i]) {
                param_info[i].min = *param_pointers[i];
            } else {
                // Try to guess parameters that make sense to go below zero
                f32 a = absolute_value(param_info[i].default_value);
                if (a < 0.5f) {
                    param_info[i].min = -1.0f;
                } else {
                    param_info[i].min = 0.0f;
                }
            }
        }
    }

    param_info[0].min = 0.5;
    param_info[0].max = 3.0;

    ParamSet defaults = {};
    defaults.params = (f32 *)calloc(num_params, sizeof(f32));
    defaults.min = (f32 *)calloc(num_params, sizeof(f32));
    defaults.max = (f32 *)calloc(num_params, sizeof(f32));
    defaults.num_params = num_params;
    for (size_t i = 0; i < num_params; i++) {
        defaults.params[i] = param_info[i].default_value;
        defaults.min[i] = param_info[i].min;
        defaults.max[i] = param_info[i].max;
    }

    CalcInfo result = {};
    result.version = GetCalcVersion();
    result.num_mods = NumMods;
    result.num_params = num_params;
    result.mods = mod_info;
    result.params = param_info;
    result.defaults = defaults;
    return result;
}

SeeCalc calc_init(CalcInfo *info)
{
    SeeCalc result = {};
    result.handle = new Calc();
    result.handle->mod_params = (f32 *)calloc(info->num_params, sizeof(f32));
    for (size_t i = 0; i < info->num_params; i++) {
        result.handle->mod_params[i] = info->params[i].default_value;
    }
    return result;
}

SkillsetRatings calc_go(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal)
{
    SkillsetRatings result = {};
    memcpy(calc->handle->mod_params, params->params, params->num_params * sizeof(float));
    vector<float> ratings = MinaSDCalc(note_data->ref, clamp_low(1e-5f, params->params[0]), goal, calc->handle);
    for (int i = 0; i < NumSkillsets; i++) {
        result.E[i] = ratings[i];
    }
    return result;
}

SkillsetRatings calc_go_with_param(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal, i32 param, f32 value)
{
    SkillsetRatings result = {};
    memcpy(calc->handle->mod_params, params->params, params->num_params * sizeof(float));
    calc->handle->mod_params[param] = value;
    vector<float> ratings = MinaSDCalc(note_data->ref, clamp_low(1e-5f, param == 0 ? value : params->params[0]), goal, calc->handle);
    for (int i = 0; i < NumSkillsets; i++) {
        result.E[i] = ratings[i];
    }
    return result;
}

void nddump(NoteData *nd, NoteData *nd2)
{
    for (i32 i = 0; i < nd->ref.size(); i++) {
        float f = nd->ref.at(i).rowTime - nd2->ref.at(i).rowTime;
        if (fabs(f) > 1e-6) {
            printf("f %d %.7f %.7f %.7f\n", i, f,  nd->ref.at(i).rowTime, nd2->ref.at(i).rowTime);
        }
        int d = nd->ref.at(i).notes - nd2->ref.at(i).notes;
        if (d) {
            printf("d %d %d\n", i, d);
        }
    }
    exit(1);
}
