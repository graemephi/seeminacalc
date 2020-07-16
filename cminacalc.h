#include <stddef.h>

typedef struct SmFile SmFile;
typedef struct Calc Calc;
typedef struct NoteData NoteData;
typedef struct TheGreatBazoinkazoinkInTheSky TheGreatBazoinkazoinkInTheSky;

enum {
    NumSkillsetRatings = 8
};

typedef union SkillsetRatings
{
    struct {
        float overall;
        float stream;
        float jumpstream;
        float handstream;
        float stamina;
        float jackspeed;
        float chordjacks;
        float technical;
    };
    float E[NumSkillsetRatings];
} SkillsetRatings;
static_assert(sizeof(SkillsetRatings) == (NumSkillsetRatings * sizeof(float)), "size mismatch");

const char *SkillsetNames[] = {
    "Overall",
    "Stream",
    "Jumpstream",
    "Handstream",
    "Stamina",
    "Jackspeed",
    "Chordjacks",
    "Technical",
};
static_assert(sizeof(SkillsetNames) == (NumSkillsetRatings * sizeof(const char *)), "size mismatch");

#ifndef __NDSTRUCTS__
typedef struct NoteInfo
{
    float rowTime;
    int notes;
} NoteInfo;
#endif

typedef struct EffectMasks
{
    unsigned char *weak;
    unsigned char *strong;
} EffectMasks;

typedef struct ModInfo
{
    const char *name;
    int num_params;
    int index;
} ModInfo;

typedef struct ParamInfo
{
    const char *name;
    int mod;
    float default_value;
    float low;
    float high;
    bool integer;
} ParamInfo;

typedef struct SeeCalc
{
    Calc *handle;
    float *params;
    size_t num_params;
} SeeCalc;

typedef struct CalcInfo
{
    int version;
    size_t num_mods;
    size_t num_params;
    ModInfo *mods;
    ParamInfo *params;
} CalcInfo;

#ifdef __cplusplus
extern "C"
{
#endif

void calculate_effects(CalcInfo *ci, SeeCalc *calc, NoteData *note_data, EffectMasks *masks);

NoteData *frobble_serialized_note_data(char *note_data, size_t length);
NoteData *frobble_note_data(NoteData *note_data, size_t length);

CalcInfo calc_info();
SeeCalc calc_init(CalcInfo *info);
void calc_set_params(SeeCalc *calc, float *params, size_t num_params);
void calc_set_param(SeeCalc *calc, size_t param, float value);
void calc_go(SeeCalc *calc, NoteData *note_data, float rate, float goal, SkillsetRatings *out);

void nddump(NoteData *nd, NoteData *nd2);

#ifdef __cplusplus
}
#endif
