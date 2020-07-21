#include <setjmp.h>

typedef struct SmParser
{
    char *buf;
    char *p;
    char *end;
    jmp_buf *env;
    String error;
} SmParser;

typedef struct SmBuffer
{
    char *buf;
    size_t len;
} SmBuffer;

typedef struct SmString
{
    i32 index;
    i32 len;
} SmString;

typedef enum {
    Tag_ArtistTranslit,
    Tag_Artist,
    Tag_Attacks,
    Tag_Background,
    Tag_Banner,
    Tag_BGChanges,
    Tag_BPMs,
    Tag_CDTitle,
    Tag_Credit,
    Tag_Delays,
    Tag_DisplayBPM,
    Tag_FGChanges,
    Tag_Genre,
    Tag_KeySounds,
    Tag_LyricsPath,
    Tag_Music,
    Tag_Notes,
    Tag_Offset,
    Tag_SampleLength,
    Tag_SampleStart,
    Tag_Selectable,
    Tag_Stops,
    Tag_SubtitleTranslit,
    Tag_Subtitle,
    Tag_TimeSignatures,
    Tag_TitleTranslit,
    Tag_Title,
    TagCount
} SmTag;

typedef struct SmTagValue
{
    SmTag tag;
    SmString str;
} SmTagValue;

typedef enum SmDifficulty
{
    Diff_Beginner,
    Diff_Easy,
    Diff_Medium,
    Diff_Hard,
    Diff_Challenge,
    Diff_Edit,
    DiffCount
} SmDifficulty;

typedef struct BPMChange
{
    f32 bpm;
    f32 bps;
    f32 beat;
    f32 row;
    f32 time;
} BPMChange;

// FLT_MAX w/o header
static const BPMChange SentinelBPM = { .beat = 3.402823466e+38f, .row = 3.402823466e+38f, .time = 3.402823466e+38f };

typedef struct SmStop
{
    f32 beat;
    f32 row;
    f32 duration;
} SmStop;

typedef struct SmRow
{
    f32 row;
    f32 time;
    u8 snap;
    union {
        u8 columns[4];
        u32 cccc;
    };
} SmRow;

typedef struct SmDiff
{
    i32 diff;

    SmString mode;
    SmString author;
    SmString desc;
    SmString radar;
    SmString notes;

    SmRow *rows;
    i32 n_rows;
} SmDiff;

typedef struct SmFile
{
    SmBuffer sm;
    SmString tags[TagCount];
    SmDiff *diffs;
    BPMChange *file_bpms;
    BPMChange *bpms;
    i32 n_bpms;
    i32 has_stops;
} SmFile;

typedef union SmFileRow
{
    u8 columns[4];
    u32 cccc;
} SmFileRow;

enum
{
    Note_Off = 0,
    Note_On = 1,
    Note_Mine = 2,
    Note_HoldStart = 4,
    Note_RollStart = 8,
    Note_HoldEnd = 16,
    Note_Lift = 32,
    Note_Fake = 64,

    Note_Tap = Note_On | Note_HoldStart | Note_RollStart
};
