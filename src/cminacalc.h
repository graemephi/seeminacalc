#include <stddef.h>
#include <stdbool.h>

typedef struct NoteData NoteData;
typedef struct TheGreatBazoinkazoinkInTheSky TheGreatBazoinkazoinkInTheSky;

#include "Etterna/Models/NoteData/NoteDataStructures.h"

typedef enum CalcPatternMod CalcPatternMod;
typedef enum CalcDiffValue CalcDiffValue;
typedef enum Skillset Skillset;

#ifdef __cplusplus
class Calc;
#else
typedef struct Calc Calc;
#endif

enum {
    NumSkillsets = NUM_Skillset
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

typedef enum AdjDiffOp
{
    AdjDiffOp_Mul,
    AdjDiffOp_Div,
    AdjDiffOp_Stam
} AdjDiffOp;

typedef struct AdjDiff
{
    AdjDiffOp op;
    CalcPatternMod mod;
    float scale, offset, pow;
    float lo, hi;
    CalcDiffValue stam_base;
    Skillset stam_ss;
    Skillset ss;  // hack cause lazy, ignored by minacalc
    bool enabled; // hack cause lazy, ignored by minacalc
} AdjDiff;
typedef struct NoteInfo NoteInfo;

typedef struct ParamSet
{
    float *params;
    float *min;
    float *max;
    size_t num_params;
    bool *pmods_used;
    size_t num_pmods_used;
    CalcDiffValue adj_diff_base[NumSkillsets];
    AdjDiff *ops;
    size_t num_ops;
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

typedef struct DebugBuffers DebugBuffers;

typedef struct DebugInfo
{
    struct {
        float *pmod[NUM_CalcPatternMod];
        float *diff[NUM_CalcDiffValue];
        float *misc[NUM_CalcDebugMisc];
        ptrdiff_t length;
    } interval_hand[2];
    struct {
        float *row_time;
        float *jack_diff;
        float *jack_stam;
        float *jack_loss;
        ptrdiff_t length;
    } jack_hand[2];
    float *interval_times;
    ptrdiff_t n_intervals;
    DebugBuffers *buffers;
} DebugInfo;

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
NoteData *frobble_serialized_note_data(char const *note_data, size_t length);

// Turns arbitrary NoteInfo into an opaque handle to C++ whateverisms.
// Note that if you aren't getting this from cache.db you probably want to
// use Etterna's code because the calc is *very* sensitive to rounding error.
// __If your note data is not bit-for-bit identical you will see error__
NoteData *frobble_note_data(NoteInfo *note_data, size_t length);
void free_note_data(NoteData *note_data);

isize note_data_row_count(NoteData *note_data);
NoteInfo const *note_data_rows(NoteData *note_data);

CalcInfo calc_info(void);

const char *file_for_param(CalcInfo *info, size_t param_index);
InlineConstantInfo *info_for_inline_constant(CalcInfo *info, size_t param_index);

SeeCalc calc_init(CalcInfo *info);
SkillsetRatings calc_go(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal);
SkillsetRatings calc_go_with_param(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal, int param, float value);
SkillsetRatings calc_go_with_rate_and_param(SeeCalc *calc, ParamSet *params, NoteData *note_data, float goal, float rate, int param, float value);
DebugInfo calc_go_debuginfo(SeeCalc *calc, ParamSet *params, NoteData *note_data, float rate);

void debuginfo_free(DebugInfo *debug_info);
size_t debuginfo_mod_index(size_t mod);

#ifdef __cplusplus
}
#endif
