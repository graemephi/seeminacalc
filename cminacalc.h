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

typedef struct EffectMasks
{
    unsigned int *weak;
    unsigned int *strong;
} EffectMasks;

typedef struct CalcInfo
{
    Calc *handle;
    int num_mods;
    int num_mod_params;
    int *num_params_for_mod;
    float *mod_params;
    const char **mod_names;
    const char **mod_param_names;

    TheGreatBazoinkazoinkInTheSky *shalhoub;
} CalcInfo;

#ifdef __cplusplus
extern "C"
{
#endif

NoteData *frobble_serialized_note_data(char *note_data, size_t length);
NoteData *frobble_sm(SmFile *sm, int diff);

CalcInfo calc_init();
void calc_set_mods(Calc *calc, float *mods);
void calc_go(Calc *calc, NoteData *note_data, float rate, float goal, SkillsetRatings *out);

EffectMasks calculate_effects(CalcInfo *ci, NoteData *note_data);

void nddump(NoteData *nd, NoteData *nd2);

#ifdef __cplusplus
}
#endif
