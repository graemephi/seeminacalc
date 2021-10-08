#ifdef __clang__
// for libs only
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include <ctype.h>

#define SQLITE3
#include "sqlite3.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

typedef struct CacheDB
{
    sqlite3 *db;
    sqlite3_stmt *note_data_stmt;
    sqlite3_stmt *file_stmt;
    sqlite3_stmt *all_stmt;
} CacheDB;

static CacheDB cache_db = {0};

typedef struct DBFile
{
    b32 ok;
    String title;
    String author;
    i32 difficulty;
    String chartkey;
    Buffer note_data;
} DBFile;

typedef struct TargetFile
{
    String key;
    String author;
    String title;
    i32 difficulty;
    i32 skillset;
    f32 rate;
    f32 target;
    f32 weight;
    Buffer note_data;
} TargetFile;

b32 db_init(const char *path)
{
    i32 result = true;
    int rc = sqlite3_open_v2(path, &cache_db.db, SQLITE_OPEN_READONLY, 0);
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

    int difficulty = sqlite3_column_int(stmt, 2);

    const void *nd = sqlite3_column_blob(stmt, 3);
    isize nd_len = sqlite3_column_bytes(stmt, 3);

    const u8 *ck = sqlite3_column_blob(stmt, 4);
    isize ck_len = sqlite3_column_bytes(stmt, 4);

    if (nd_len == 0) {
        return;
    }

    if (title_len) {
        out->title.len = buf_printf(out->title.buf, "%.*s", title_len, title);
    } else {
        out->title = S("");
    }

    assert(ck_len);
    out->chartkey.len = buf_printf(out->chartkey.buf, "%.*s", ck_len, ck);

    if (author_len) {
        out->author.len = buf_printf(out->author.buf, "%.*s", author_len, author);
    } else {
        out->author = S("");
    }

    out->difficulty = difficulty;

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

// Extremely jank xml reader
typedef struct {
    b32 ok;
    u8 *ptr;
    u8 *end;
    u8 *last_next;
    String *stack;
    u8 *current_tag_end;
} XML;

XML xml_begin(Buffer buf)
{
    XML result = {0};
    result.ok = buf.buf != 0;
    result.ptr = buf.buf;
    result.end = result.ptr + buf.len;
    return result;
}

b32 xml_next(XML *x)
{
    if (x->ok) {
        u8 *p = x->ptr + 1;
        u8 *end = x->end;
        while (p < end) {
            while (p < end && *p != '<') {
                p++;
            }
            if (p[1] == '!') {
                // <!-- comment -->
                while ((p+2) < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) {
                    p++;
                }
            } else {
                break;
            }
        }
        x->ok = p < end;
        x->last_next = p;
        x->ptr = p;
    }
    return x->ok;
}

String xml_token(u8 *p)
{
    String result = { .buf = p };
    while (!isspace(*p) && *p != '<' && *p != '>') {
        p++;
    }
    result.len = p - (u8 *)result.buf;
    return result;
}

b32 xml_open(XML *x, String tag)
{
    if (x->ok) {
        do {
            if (buf_len(x->stack) >= 0 && x->ptr[1] == '/' && strings_are_equal(buf_last(x->stack), xml_token(x->ptr + 2))) {
                return false;
            }
            if (strings_are_equal(tag, xml_token(x->ptr + 1))) {
                buf_push(x->stack, tag);
                u8 *p = x->ptr + 1;
                while (p < x->end && *p != '<' && *p != '>') {
                    p++;
                }
                x->current_tag_end = p;
                return true;
            }
        } while (xml_next(x));
    }
    return x->ok;
}

b32 xml_close(XML *x, String tag)
{
    if (buf_len(x->stack) <= 0 || strings_are_equal(tag, buf_last(x->stack)) == false) {
        x->ok = false;
    }
    if (x->ok) {
        u8 *here = x->ptr;
        do {
            if (x->ptr != here && strings_are_equal(tag, xml_token(x->ptr + 1))) {
                // nested tags of the same type are not allowed
                x->ok = false;
                break;
            }
            String tok = xml_token(x->ptr + 2);
            if (x->ptr[1] == '/' && tok.buf[tok.len] == '>' && strings_are_equal(tag, tok)) {
                buf_pop(x->stack);
                break;
            }
        } while (xml_next(x));
    }

    x->current_tag_end = 0;
    return x->ok;
}

b32 next_char(u8 **pp, u8 c)
{
    u8 *p = *pp;
    while (*p && *p != c) {
        p++;
    }
    *pp = p;
    return *p == c;
}

b32 next_char_skip_whitespace(u8 **pp, u8 c)
{
    u8 *p = *pp;
    while (*p && (*p != c || *p <= ' ')) {
        p++;
    }
    *pp = p;
    return *p == c;
}

String xml_attr(XML *x, String key)
{
    String result = {0};

    if (!x->current_tag_end) {
        goto bail;
    }

    u8 *begin = x->ptr;
    u8 *end = x->current_tag_end;

    if (x->ok) {
        u8 *p = begin;

        while (p < end && !string_equals_cstr(key, p)) {
            p++;
        }
        if (p == end) {
            goto bail;
        }
        if (!next_char_skip_whitespace(&p, '=')) {
            goto bail;
        }
        if (!next_char_skip_whitespace(&p, '\'')) {
            goto bail;
        }
        p++;
        u8 *start_of_value = p;
        if (!next_char(&p, '\'')) {
            goto bail;
        }
        u8 *end_of_value = p;
        result = (String) { start_of_value, end_of_value - start_of_value };
    }

    if (0) bail: {
        x->ok = false;
    }

    return result;
}

TargetFile *load_test_files(char const *db, char const *xml_path)
{
    db_init(db);
    Buffer xml_file = read_file(xml_path);

    if (!cache_db.db) {
        printf("Unable to load db (path: %s)\n", db);
    }

    if (xml_file.len == 0) {
        printf("Unable to load test list (path: %s)\n", xml_path);
    }

    if (!cache_db.db || xml_file.len == 0) {
        return 0;
    }

    push_allocator(scratch);
    TargetFile *result = 0;

    XML xml = xml_begin(xml_file);
    while (xml_open(&xml, S("CalcTestList"))) {
        String skillset = xml_attr(&xml, S("Skillset"));
        i32 ss = string_to_i32(skillset);

        if (ss > 0 && ss < NumSkillsets) {
            while (xml_open(&xml, S("Chart"))) {
                String key = xml_attr(&xml, S("aKey"));
                String rate = xml_attr(&xml, S("bRate"));
                String target = xml_attr(&xml, S("cTarget"));

                String ztkey = {0};
                ztkey.len = buf_printf(ztkey.buf, "%.*s", key.len, key.buf);
                DBFile dbf = db_get_file(ztkey.buf);

                if (xml.ok && dbf.ok) {
                    buf_push(result, (TargetFile) {
                        .key = dbf.chartkey,
                        .author = dbf.author,
                        .title = dbf.title,
                        .difficulty = dbf.difficulty,
                        .skillset = ss,
                        .rate = string_to_f32(rate),
                        .target = string_to_f32(target),
                        .weight = 1.0f,
                        .note_data = dbf.note_data,
                    });
                } else {
                    printf("Missing %.*s\n", (i32)key.len, key.buf);
                }

                xml_close(&xml, S("Chart"));
            }
        } else {
            printf("CalcTestList parse error: skillset: %.*s (should be 1-7)\n", (i32)skillset.len, skillset.buf);
        }

        xml_close(&xml, S("CalcTestList"));
    }

    pop_allocator();
    return result;
}
