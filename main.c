#include "bottom.h"

#include "cminacalc.h"
#include "sm.h"

#pragma float_control(precise, on, push)
#include "sm.c"
#pragma float_control(pop)

#include "sqlite3.c"

u64 time_now()
{
    u64 result = 0;
    QueryPerformanceCounter((void *)&result);
    return result;
}

f64 time_between(u64 a, u64 b)
{
    static u64 qpf = 0;
    if (qpf == 0) {
        QueryPerformanceFrequency((void *)&qpf);
    }
    return (f64)(b - a) / (f64)qpf;
}

typedef struct CacheDB
{
    sqlite3 *db;
    sqlite3_stmt *note_data_stmt;
} CacheDB;

CacheDB cachedb_init(const char *path)
{
    CacheDB result;
    int rc = sqlite3_open(path, &result.db);
    if (rc) {
        goto err;
    }

    char query[] = "select serializednotedata from steps where chartkey=?;";
    rc = sqlite3_prepare_v2(result.db, query, sizeof(query), &result.note_data_stmt, 0);
    if (rc) {
        goto err;
    }

    return result;

err:
    fprintf(stderr, "Sqlite3 oops: %s\n", sqlite3_errmsg(result.db));
    sqlite3_close(result.db);
    result = (CacheDB) {0};
    return result;
}

Buffer get_steps_from_db(CacheDB *db, char *key)
{
    Buffer result = (Buffer) {0};
    sqlite3_bind_text(db->note_data_stmt, 1, key, -1, 0);
    sqlite3_step(db->note_data_stmt);
    const void *blob = sqlite3_column_blob(db->note_data_stmt, 0);
    size_t len = sqlite3_column_bytes(db->note_data_stmt, 0);

    result.buf = calloc(len, sizeof(char));
    memcpy(result.buf, blob, len);
    result.len = len;
    result.cap = len;

    sqlite3_reset(db->note_data_stmt);
    return result;
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        printf("gimme a db\n");
        exit(1);
    }

    CacheDB db = cachedb_init(argv[1]);
    Buffer cached = get_steps_from_db(&db, "X9a609c6dd132d807b2abc5882338cb9ebbec320d");
    NoteData *nd = frobble_serialized_note_data(cached.buf, cached.len);

    CalcInfo ci = calc_init();

    u64 a = time_now();
    EffectMasks em = calculate_effects(&ci, nd);
    u64 b = time_now();
    printf("%f\n", time_between(a, b));

    return 0;
}
