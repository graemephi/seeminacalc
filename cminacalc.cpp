
#include <memory>
#include <vector>
#include <string_view>
#include <cstddef>

using std::vector;

// Note on updating the calc: not quite drag-and-drop. grep for "Stupud hack"
#include "Etterna/MinaCalc/MinaCalc.h"
#include "Etterna/MinaCalc/MinaCalc.cpp"

#include "common.h"
#include "cminacalc.h"

static f32 absolute_value(f32 a)
{
    return (a >= 0) ? a : -a;
}

const char *ModNames[] = {
    "StreamMod",
    "JSMod",
    "HSMod",
    "CJMod",
    "CJDensityMod",
    "OHJumpModGuyThing",
    "CJOHJumpMod",
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

static f32 rating_floor(f32 v)
{
    return (f32)((i32)(v * 100.0f)) / 100.f;
}

void calculate_effects(CalcInfo *info, SeeCalc *calc, NoteData *note_data, EffectMasks *masks)
{
    assert(masks->weak);
    assert(masks->strong);

    SkillsetRatings default_ratings = {};
    calc_go(calc, &info->defaults, note_data, 1.0f, 0.93f, &default_ratings);
    for (i32 r = 0; r < NumSkillsetRatings; r++) {
        default_ratings.E[r] = rating_floor(default_ratings.E[r]);
    }

    size_t num_params = info->num_params;
    f32 *params = (f32 *)calloc(num_params, sizeof(f32));
    memcpy(params, info->defaults.params, num_params * sizeof(f32));
    ParamSet params_set = {};
    params_set.params = params;
    params_set.num_params = num_params;

    b8 changed[NumSkillsetRatings] = {};

    SkillsetRatings ratings = {};
    for (int i = 0; i < num_params; i++) {
        // stronk. they react to small changes at 93%
        {
            if (info->params[i].default_value == 0) {
                params[i] = 1;
            } else if (info->params[i].default_value > info->params[i].min) {
                params[i] = info->params[i].default_value * 0.95f;
            } else {
                params[i] = info->params[i].default_value * 1.05f;
            }

            calc_go(calc, &params_set, note_data, 1.0f, 0.93f, &ratings);

            for (int r = 0; r < NumSkillsetRatings; r++) {
                b32 changed = rating_floor(ratings.E[r]) != default_ratings.E[r];
                masks->strong[i] |= (changed << r);
            }
        }

        // weak. they react to big changes at 96.5%, and not small changes at 93%
        {
            f32 distance_from_low = info->params[i].default_value - info->params[i].min;
            f32 distance_from_high = info->params[i].max - info->params[i].max;
            assert(info->params[i].max >= info->params[i].min);
            if (info->params[i].default_value == 0) {
                params[i] = 100;
            } else if (distance_from_low > distance_from_high) {
                params[i] = info->params[i].min;
            } else {
                params[i] = info->params[i].max;
            }

            calc_go(calc, &params_set, note_data, 1.0f, 0.93f, &ratings);

            masks->weak[i] = masks->strong[i];
            for (i32 r = 0; r < NumSkillsetRatings; r++) {
                b32 changed = rating_floor(ratings.E[r]) != default_ratings.E[r];
                masks->weak[i] |= (changed << r);
            }
        }

        params[i] = info->params[i].default_value;
    }

    free(params);
}

NoteData *frobble_serialized_note_data(char *note_data, size_t length)
{
    auto result = new std::vector<NoteInfo>((NoteInfo *)note_data, (NoteInfo *)(note_data + length));
    return (NoteData *)result;
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
        &shalhoub._s._params,
        &shalhoub._js._params,
        &shalhoub._hs._params,
        &shalhoub._cj._params,
        &shalhoub._cjd._params,
        &shalhoub._ohj._params,
        &shalhoub._cjohj._params,
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
        &shalhoub._tt2._params
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

void calc_go(SeeCalc *calc, ParamSet *params, NoteData *note_data, float rate, float goal, SkillsetRatings *out)
{
    // Stupud hack related. If that wasn't necessary, we could do better than
    // memcpying every time, but it probably doesn't matter
    memcpy(calc->handle->mod_params, params->params, params->num_params * sizeof(float));
    vector<float> ratings = MinaSDCalc(note_data->ref, rate, goal, calc->handle);
    for (int i = 0; i < NumSkillsetRatings; i++) {
        out->E[i] = ratings[i];
    }
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
