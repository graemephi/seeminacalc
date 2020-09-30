#if !defined(SEEMINACALC)
#ifdef __clang__
// for libs only
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include "sqlite3.c"
// The perils of including sqlite3 in your translation unit
// sqlite3.c also disables certain warnings.
#if defined(DEBUG)
#undef NDEBUG
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "stb_sprintf.h"

#define SOKOL_IMPL
#include "sokol/sokol_time.h"

#include "thread.h"
#include "bottom.h"
#include "cminacalc.h"
#include "sm.h"

#include "cachedb.h"

typedef struct CacheDB
{
    sqlite3 *db;
    sqlite3_stmt *note_data_stmt;
    sqlite3_stmt *file_stmt;
    sqlite3_stmt *all_stmt;
} CacheDB;

static CacheDB cache_db = {0};

b32 db_init(const char *path)
{
    i32 result = true;
    int rc = sqlite3_open(path, &cache_db.db);
    if (rc) {
        goto err;
    }

    char note_query[] = "select serializednotedata from steps where chartkey=?;";
    rc = sqlite3_prepare_v2(cache_db.db, note_query, sizeof(note_query), &cache_db.note_data_stmt, 0);
    if (rc) {
        goto err;
    }

    char file_query[] = "select songs.title, songs.credit, steps.difficulty, steps.serializednotedata, steps.chartkey from steps inner join songs on songs.id = steps.songid where chartkey=?;";
    rc = sqlite3_prepare_v2(cache_db.db, file_query, sizeof(file_query), &cache_db.file_stmt, 0);
    if (rc) {
        goto err;
    }

    char all_query[] = "select songs.title, songs.credit, steps.difficulty, steps.serializednotedata, steps.chartkey from steps inner join songs on songs.id = steps.songid where steps.stepstype=\"dance-single\"";
    rc = sqlite3_prepare_v2(cache_db.db, all_query, sizeof(all_query), &cache_db.all_stmt, 0);
    if (rc) {
        goto err;
    }

    return result;

err:
    fprintf(stderr, "sqlite3 oops: %s\n", sqlite3_errmsg(cache_db.db));
    sqlite3_close(cache_db.db);
    cache_db = (CacheDB) {0};
    result = false;
    return result;
}

Buffer db_get_steps(char *key)
{
    Buffer result = (Buffer) {0};
    sqlite3_bind_text(cache_db.note_data_stmt, 1, key, -1, 0);
    sqlite3_step(cache_db.note_data_stmt);
    const void *blob = sqlite3_column_blob(cache_db.note_data_stmt, 0);
    size_t len = sqlite3_column_bytes(cache_db.note_data_stmt, 0);

    result.buf = calloc(len, sizeof(char));
    memcpy(result.buf, blob, len);
    result.len = len;
    result.cap = len;

    sqlite3_reset(cache_db.note_data_stmt);
    return result;
}

void dbfile_from_stmt(sqlite3_stmt *stmt, DBFile *out)
{
    out->ok = false;
    assert(stmt == cache_db.file_stmt || stmt == cache_db.all_stmt);
    sqlite3_step(stmt);

    const u8 *title = sqlite3_column_text(stmt, 0);
    isize title_len = sqlite3_column_bytes(stmt, 0);

    const u8 *author = sqlite3_column_text(stmt, 1);
    isize author_len = sqlite3_column_bytes(stmt, 1);

    int diff = sqlite3_column_int(stmt, 2);

    const void *nd = sqlite3_column_blob(stmt, 3);
    isize nd_len = sqlite3_column_bytes(stmt, 3);

    const u8 *ck = sqlite3_column_blob(stmt, 4);
    isize ck_len = sqlite3_column_bytes(stmt, 4);

    if (nd_len == 0) {
        return;
    }

    out->title.len = buf_printf(out->title.buf, "%.*s", title_len, title);
    out->chartkey.len = buf_printf(out->chartkey.buf, "%.*s", ck_len, ck);

    if (author_len) {
        out->author.len = buf_printf(out->author.buf, "%.*s", author_len, author);
    } else {
        out->author.len = 0;
    }

    out->diff = SmDifficultyStrings[diff];

    if (nd_len > out->note_data.cap) {
        out->note_data.buf = realloc(out->note_data.buf, nd_len * 4);
        out->note_data.cap = nd_len * 4;
    }
    memcpy(out->note_data.buf, nd, nd_len);
    out->note_data.len = nd_len;

    out->ok = true;
}

DBFile db_get_file(char *key)
{
    sqlite3_bind_text(cache_db.file_stmt, 1, key, -1, 0);
    DBFile result = {0};
    dbfile_from_stmt(cache_db.file_stmt, &result);
    sqlite3_reset(cache_db.file_stmt);
    return result;
}

b32 db_iter_next(DBFile *out)
{
    buf_clear(out->title.buf);
    buf_clear(out->author.buf);
    buf_clear(out->chartkey.buf);
    dbfile_from_stmt(cache_db.all_stmt, out);
    return sqlite3_stmt_busy(cache_db.all_stmt);
}


typedef struct { String id; SkillsetRatings delta; } McBlooper;
i32 cmp_idx = 0;
i32 mcblooper_cmp(McBlooper const *a, McBlooper const *b)
{
    return (fabsf(a->delta.E[cmp_idx]) > fabsf(b->delta.E[cmp_idx])) ? -1
         : (fabsf(a->delta.E[cmp_idx]) < fabsf(b->delta.E[cmp_idx])) ?  1
         : 0;
}

float test_mcbloop_multiplier = 0.9f;

void statdump(void) {
    isize bignumber = 100*1024*1024;
    scratch_stack = stack_make(malloc(bignumber), bignumber);
    permanent_memory_stack = stack_make(malloc(bignumber), bignumber);

    isize i = 0;
    stm_setup();
    CalcInfo ci = calc_info();
    SeeCalc sc = calc_init(&ci);
    u64 a = stm_now();
    McBlooper *deltas = 0;
    SkillsetRatings avg = {0};
    SkillsetRatings maxi = {0};
    buf_reserve(deltas, 23160);
    isize count = 0;
    for (DBFile f = {0}; db_iter_next(&f);) {
        if (f.note_data.len > 0) {
            NoteData *nd = frobble_serialized_note_data(f.note_data.buf, f.note_data.len);
            test_mcbloop_multiplier = 0.9f;
            SkillsetRatings ssr1 = calc_go(&sc, &ci.defaults, nd, 0.93f);
            test_mcbloop_multiplier = 0.f;
            SkillsetRatings ssr2 = calc_go(&sc, &ci.defaults, nd, 0.93f);

            if (ssr1.overall != 0 || ssr2.overall != 0) {
                assert(ssr1.overall > 0);
                assert(ssr2.overall > 0);
                deltas[i].id.len = buf_printf(deltas[i].id.buf, "%.*s (%.*s%.*s%.*s) %.*s",
                    f.title.len, f.title.buf,
                    f.author.len, f.author.buf,
                    f.author.len == 0 ? 0 : 2, ", ",
                    f.diff.len, f.diff.buf,
                    f.chartkey.len, f.chartkey.buf
                );
                for (isize ss = 0; ss < NumSkillsets; ss++) {
                    deltas[i].delta.E[ss] = ssr1.E[ss] - ssr2.E[ss];
                    avg.E[ss] += deltas[i].delta.E[ss];
                    maxi.E[ss] = fabsf(maxi.E[ss]) > fabsf(deltas[i].delta.E[ss]) ? maxi.E[ss] : deltas[i].delta.E[ss];
                }
                i++;
                count += ssr1.overall == ssr2.overall;
            }
        }

        if (((i+1) % 250) == 0) {
            printf(".");
            fflush(0);
        }
        if (((i+1) % 1250) == 0) {
            printf(" %zd ", i);
            for (isize ss = 0; ss < NumSkillsets; ss++) {
                printf("%.2f ", (f64)avg.E[ss] / (f64)i);
            }
            printf("\n");
        }
    }
    printf("%f\n", (f64)count / (f64)i);
    u64 b = stm_now();
    printf("\n\n");
    printf("\n %zd %f\n avg ", i, stm_sec(b - a));
    for (isize ss = 0; ss < NumSkillsets; ss++) {
        printf("%.2f ", (f64)avg.E[ss] / (f64)i);
    }
    printf("\n max ");
    for (isize ss = 0; ss < NumSkillsets; ss++) {
        printf("%.2f ", (f64)maxi.E[ss]);
    }
    for (isize ss = 0; ss < NumSkillsets; ss++) {
        printf("\n=============================================\n%s\n", SkillsetNames[ss]);
        cmp_idx = ss;
        qsort(deltas, i, sizeof(*deltas), (void *)mcblooper_cmp);
        for (isize i = 0; i < 20; i++) {
            printf("%zd %4.2f %4.2f %s\n", i+1, (f64)deltas[i].delta.E[ss], (f64)deltas[i].delta.overall, deltas[i].id.buf);
        }
    }
}

int main(int argc, char **argv)
{
    db_init(argv[1]);
    statdump();
}

#endif // !defined(SEEMINACALC)
