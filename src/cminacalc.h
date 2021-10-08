#include <stddef.h>
#include <stdbool.h>

typedef struct NoteData NoteData;
typedef struct TheGreatBazoinkazoinkInTheSky TheGreatBazoinkazoinkInTheSky;

#ifdef __cplusplus
class Calc;
#else
typedef struct Calc Calc;
#endif

enum {
    NumSkillsets = 8
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
    float E[NumSkillsets];
} SkillsetRatings;

static const char *SkillsetNames[] = {
    "Overall",
    "Stream",
    "Jumpstream",
    "Handstream",
    "Stamina",
    "Jackspeed",
    "Chordjacks",
    "Technical",
};

#ifndef __NDSTRUCTS__
typedef struct NoteInfo
{
    int notes;
    float rowTime;
} NoteInfo;
#endif

typedef struct ParamSet
{
    float *params;
    float *min;
    float *max;
    size_t num_params;
} ParamSet;

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
    float min;
    float max;
    bool integer;
    bool constant;
    bool optimizable;
    bool fake;
} ParamInfo;

typedef struct SeeCalc
{
    Calc *handle;
} SeeCalc;

typedef struct CalcInfo
{
    int version;
    ptrdiff_t num_mods;
    ptrdiff_t num_params;
    ModInfo *mods;
    ParamInfo *params;
    ParamSet defaults;
} CalcInfo;

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct InlineConstantInfo InlineConstantInfo;

// Turns the SQLite binary blob from the `serializednotedata` column in the `steps`
// table of the cache db into an opaque handle to C++ whateverisms.
NoteData *frobble_serialized_note_data(char *note_data, size_t length);

// Turns arbitrary NoteInfo into an opaque handle to C++ whateverisms.
// Note that if you aren't getting this from cache.db you probably want to
// use Etterna's code because the calc is *very* sensitive to rounding error.
// __If your note data is not bit-for-bit identical you will see error__
NoteData *frobble_note_data(NoteInfo *note_data, size_t length);
void free_note_data(NoteData *note_data);

CalcInfo calc_info(void);
const char *file_for_param(CalcInfo *info, size_t param_index);
InlineConstantInfo *info_for_inline_constant(CalcInfo *info, size_t param_index);
SeeCalc calc_init(CalcInfo *info);
SkillsetRatings calc_go(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal);
SkillsetRatings calc_go_with_param(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal, int param, float value);
SkillsetRatings calc_go_with_rate_and_param(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal, float rate, int param, float value);

void nddump(NoteData *nd, NoteData *nd2);

#ifdef __cplusplus
}
#endif