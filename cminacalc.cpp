
#include <vector>
#include <string_view>

#include "basetypes.h"
#include "cminacalc.h"

using std::vector;

// Note on updating the calc: not quite drag-and-drop. grep for "Stupud hack"
#include "Etterna/MinaCalc/MinaCalc.h"
#include "Etterna/MinaCalc/MinaCalc.cpp"

#include "sm.h"

char *ModNames[] = {
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

EffectMasks calculate_effects(CalcInfo *ci, NoteData *note_data)
{
    SkillsetRatings default_ratings = {};
    Calc *calc = ci->handle;
    int num_mod_params = ci->num_mod_params;
    float *default_mods = calc->mod_params;

    calc_go(calc, note_data, 1.0f, 0.93f, &default_ratings);

    float *mods = (float *)calloc(num_mod_params, sizeof(float));
    memcpy(mods, default_mods, num_mod_params * sizeof(float));

    b32 changed[NumSkillsetRatings] = {};

    EffectMasks result;
    result.weak = (unsigned int *)calloc(num_mod_params, sizeof(unsigned int));
    result.strong = (unsigned int *)calloc(num_mod_params, sizeof(unsigned int));

    SkillsetRatings ratings = {};
    for (int i = 0; i < num_mod_params; i++) {
        // stronk
        {
            mods[i] = default_mods[i] * 0.8f;
            calc_set_mods(calc, mods);
            for (int r = 0; r < NumSkillsetRatings; r++) {
                changed[r] = 0;
            }

            calc_go(calc, note_data, 1.0f, 0.93f, &ratings);

            for (int r = 0; r < NumSkillsetRatings; r++) {
                changed[r] |= (ratings.E[r] != default_ratings.E[r]);
            }

            mods[i] = default_mods[i] * 1.2f;
            calc_set_mods(calc, mods);
            calc_go(calc, note_data, 1.0f, 0.93f, &ratings);

            for (int r = 0; r < NumSkillsetRatings; r++) {
                changed[r] |= (ratings.E[r] != default_ratings.E[r]);
            }

            for (int r = 0; r < NumSkillsetRatings; r++) {
                result.strong[i] |= (changed[r] << r);
            }
        }

        // weak
        {
            mods[i] = 0.f;
            calc_set_mods(calc, mods);
            for (int r = 0; r < NumSkillsetRatings; r++) {
                changed[r] = 0;
            }

            calc_go(calc, note_data, 1.0f, 0.93f, &ratings);

            for (int r = 0; r < NumSkillsetRatings; r++) {
                changed[r] |= (ratings.E[r] != default_ratings.E[r]);
            }

            mods[i] = default_mods[i] * 10.f;
            calc_set_mods(calc, mods);
            calc_go(calc, note_data, 1.0f, 0.93f, &ratings);

            for (int r = 0; r < NumSkillsetRatings; r++) {
                changed[r] |= (ratings.E[r] != default_ratings.E[r]);
            }

            for (int r = 0; r < NumSkillsetRatings; r++) {
                result.weak[i] |= result.strong[i] || (changed[r] << r);
            }
        }

        mods[i] = default_mods[i];
    }

    free(mods);
    return result;
}

// Turns the SQLite binary blob from the `serializednotedata` column in the `steps`
// table of the cache db into an opaque handle to C++ whateverisms
NoteData *frobble_serialized_note_data(char *note_data, size_t length)
{
    auto result = new std::vector<NoteInfo>((NoteInfo *)note_data, (NoteInfo *)(note_data + length));
    return (NoteData *)result;
}

static unsigned int row_bits(SmRow r)
{
    return ((unsigned int)((r.columns[0] & NoteTap) != 0) << 0)  // left
         + ((unsigned int)((r.columns[1] & NoteTap) != 0) << 1)  // left
         + ((unsigned int)((r.columns[2] & NoteTap) != 0) << 2)  // right
         + ((unsigned int)((r.columns[3] & NoteTap) != 0) << 3); // right
}

#pragma float_control(precise, on, push)
NoteData *frobble_sm(SmFile *sm, int diff)
{
    auto result = new std::vector<NoteInfo>(sm->diffs[diff].n_rows);

    SmRow *r = sm->diffs[diff].rows;

    SmString offset_str = sm->tag_values[Tag_Offset];
    float offset = strtof(&sm->sm.buf[offset_str.index], 0);

    for (int i = 0; i < sm->diffs[diff].n_rows; i++) {
        if (row_bits(r[i])) {
            offset += r[i].time;
            break;
        }
    }

    int inserted_index = 0;
    for (int i = 0; i < sm->diffs[diff].n_rows; i++) {
        unsigned int notes = row_bits(r[i]);
        if (notes) {
            result->at(inserted_index++) = {
                .notes = notes,
                .rowTime = (r[i].time - offset) + offset // HAHAHA
            };
        }
    }

    result->resize(inserted_index);
    return (NoteData *)result;
}
#pragma float_control(pop)

CalcInfo calc_init()
{
    CalcInfo result = {};
    result.handle = new Calc;
    result.mod_names = (const char **)ModNames;
    result.num_params_for_mod = (int *)calloc(NumMods, sizeof(int));

    // We keep this just so the param strings aren't freed :)
    result.shalhoub = new TheGreatBazoinkazoinkInTheSky(*result.handle);
    const auto& shalhoub = *result.shalhoub;

    // Yea yea. Blame minacalc
    int i = 0;
    int n = 0;
    n += (result.num_params_for_mod[i++] = (int)shalhoub._s._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._js._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._hs._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._cj._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._cjd._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._ohj._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._cjohj._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._bal._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._oht._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._voht._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._ch._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._rm._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._wrb._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._wrr._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._wrjt._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._wra._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._fj._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._tt._params.size());
    n += (result.num_params_for_mod[i++] = (int)shalhoub._tt2._params.size());

    result.mod_params = (float *)calloc(n, sizeof(float));
    result.mod_param_names = (const char **)calloc(n, sizeof(char *));
    float *mp = result.mod_params;
    const char **mpn = result.mod_param_names;
    for (const auto &p : shalhoub._s._params)     { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._js._params)    { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._hs._params)    { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._cj._params)    { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._cjd._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._ohj._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._cjohj._params) { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._bal._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._oht._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._voht._params)  { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._ch._params)    { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._rm._params)    { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._wrb._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._wrr._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._wrjt._params)  { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._wra._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._fj._params)    { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._tt._params)    { *mp++ = *p.second; *mpn++ = p.first.c_str(); }
    for (const auto &p : shalhoub._tt2._params)   { *mp++ = *p.second; *mpn++ = p.first.c_str(); }

    result.handle->mod_params = result.mod_params;
    result.num_mods = NumMods;
    result.num_mod_params = n;
    return result;
}

void calc_set_mods(Calc *calc, float *mods)
{
    // In principle we SHOULD be able to set the internal parameters out here, once, instead of doing it later inside ulbu every time.
    // SO the API will remain as it is *bangs gavel*
    calc->mod_params = mods;
}

void calc_go(Calc *calc, NoteData *note_data, float rate, float goal, SkillsetRatings *out)
{
    vector<float> ratings = MinaSDCalc(note_data->ref, rate, goal, calc);
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
