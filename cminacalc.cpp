
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

void calculate_effects(CalcInfo *info, SeeCalc *calc, NoteData *note_data, EffectMasks *masks)
{
    assert(masks->weak);
    assert(masks->strong);

    SkillsetRatings default_ratings = {};
    calc_go(calc, note_data, 1.0f, 0.93f, &default_ratings);

    int num_params = info->num_params;
    float *params = (float *)calloc(num_params, sizeof(float));
    memcpy(params, calc->params, num_params * sizeof(float));

    b8 changed[NumSkillsetRatings] = {};

    SkillsetRatings ratings = {};
    for (int i = 0; i < num_params; i++) {
        // stronk. they react to small changes at 93%
        {
            if (info->params[i].default_value == 0) {
                params[i] = 1;
            } else if (info->params[i].default_value > info->params[i].low) {
                params[i] = info->params[i].default_value * 0.95f;
            } else {
                params[i] = info->params[i].default_value * 1.05f;
            }

            calc_set_params(calc, params, num_params);
            calc_go(calc, note_data, 1.0f, 0.93f, &ratings);

            for (int r = 0; r < NumSkillsetRatings; r++) {
                masks->strong[i] |= (changed[r] << r);
            }
        }

        // weak. they react to big changes at 96.5%, and not small changes at 93%
        {
            float distance_from_low = info->params[i].default_value - info->params[i].low;
            float distance_from_high = info->params[i].high - info->params[i].high;
            assert(info->params[i].high >= info->params[i].low);
            if (info->params[i].default_value == 0) {
                params[i] = 100;
            } else if (distance_from_low > distance_from_high) {
                params[i] = info->params[i].low;
            } else {
                params[i] = info->params[i].high;
            }

            calc_set_params(calc, params, num_params);
            calc_go(calc, note_data, 1.0f, 0.93f, &ratings);

            for (int r = 0; r < NumSkillsetRatings; r++) {
                masks->weak[i] |= masks->strong[i] || (changed[r] << r);
            }
        }

        params[i] = info->params[i].default_value;
    }

    free(params);
}

// Turns the SQLite binary blob from the `serializednotedata` column in the `steps`
// table of the cache db into an opaque handle to C++ whateverisms.
NoteData *frobble_serialized_note_data(char *note_data, size_t length)
{
    auto result = new std::vector<NoteInfo>((NoteInfo *)note_data, (NoteInfo *)(note_data + length));
    return (NoteData *)result;
}

// Turns arbitrary NoteInfo into an opaque handle to C++ whateverisms.
// Note that if you aren't getting this from cache.db you probably want to
// use Etterna's code because the calc is *very* sensitive to rounding error.
// __If your note data is not bit-for-bit identical you will see error__
NoteData *frobble_note_data(NoteInfo *note_data, size_t length)
{
    return frobble_serialized_note_data((char *)note_data, length * sizeof(NoteInfo));
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
            param_info_cursor->mod = i;
            param_info_cursor->default_value = *p.second;
            param_info_cursor++;

            param_pointers.push_back(p.second);
        }
    }

    // Test for clamping and int-ing
    for (isize i = 0; i < num_params; i++) {
        *param_pointers[i] = absolute_value(param_info[i].default_value) * 100.5438f;
    }

    shalhoub.setup_agnostic_pmods();
    shalhoub.setup_dependent_mods();

    for (isize i = 0; i < num_params; i++) {
        f32 test_value = absolute_value(param_info[i].default_value) * 100.5438f;
        if (test_value != *param_pointers[i]) {
            param_info[i].high = *param_pointers[i];
        } else {
            f32 a = absolute_value(param_info[i].default_value);
            if (a < 1.0f) {
                param_info[i].high = 1.0f;
            } else {
                param_info[i].high = absolute_value(param_info[i].default_value) * 2.0f;
            }
        }

        param_info[i].integer = ((f32)(i32)test_value) == *param_pointers[i];
    }

    // Test for clamping low
    for (isize i = 0; i < num_params; i++) {
        *param_pointers[i] = absolute_value(param_info[i].default_value) * -100.5438f;
    }

    shalhoub.setup_agnostic_pmods();
    shalhoub.setup_dependent_mods();

    for (isize i = 0; i < num_params; i++) {
        f32 test_value = absolute_value(param_info[i].default_value) * -100.5438f;
        if (test_value != *param_pointers[i]) {
            param_info[i].low = *param_pointers[i];
        } else {
            // Try to guess parameters that make sense to go below zero
            f32 a = absolute_value(param_info[i].default_value);
            if (a < 1.0f) {
                param_info[i].low = -1.0f;
            } else {
                param_info[i].low = 0.0f;
            }
        }
    }

    CalcInfo result = {};
    result.version = GetCalcVersion();
    result.num_mods = NumMods;
    result.num_params = num_params;
    result.mods = mod_info;
    result.params = param_info;
    return result;
}

SeeCalc calc_init(CalcInfo *info)
{
    SeeCalc result = {};
    result.handle = new Calc();
    result.params = (float *)calloc(info->num_params, sizeof(float));
    result.num_params = info->num_params;
    result.handle->mod_params = result.params;
    for (isize i = 0; i < result.num_params; i++) {
        result.params[i] = info->params[i].default_value;
    }
    return result;
}

void calc_set_params(SeeCalc *calc, float *params, size_t num_params)
{
    // (Stupud hack related) In principle we SHOULD be able to set the internal
    // parameters out here, once, instead of doing it later inside ulbu every
    // time. SO the API will remain as it is *bangs gavel*
    if (NEVER(num_params != calc->num_params)) {
        return;
    }

    memcpy(calc->params, params, sizeof(f32) * num_params);
}

void calc_set_param(SeeCalc *calc, i32 index, float value)
{
    if (NEVER(index < 0 || index >= calc->num_params)) {
        return;
    }

    calc->params[index] = value;
}

void calc_go(SeeCalc *calc, NoteData *note_data, float rate, float goal, SkillsetRatings *out)
{
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
            printf("f %d %f\n", i, f);
        }
        int d = nd->ref.at(i).notes - nd2->ref.at(i).notes;
        if (d) {
            printf("d %d %d\n", i, d);
        }
    }
    exit(1);
}
