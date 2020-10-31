// Utility program. This takes the big list of chartkeys somewhere below and dumps the note data into a .h file as static byte arrays.
// Usage:
//   Build with
//       make cachedb
//
//   Run with
//       ./build/cachedb/cachedb /path/to/cache/db
//
// It will overwrite cachedb.gen.c. `make all` will build cachedb but _not_ run it; cachedb.gen.c is checked into the repo.

#ifdef __clang__
// for libs only
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#define SQLITE3
#include "sqlite3.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if !defined(SEEMINACALC)
#include "stb_sprintf.h"

#define SOKOL_IMPL
#include "sokol/sokol_time.h"

#include "thread.h"
#include "bottom.h"
#include "cminacalc.h"
#include "sm.h"
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

#if defined(SEEMINACALC)
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

// todo: xml_tokenize up front
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
        String skillset = xml_attr(&xml, S("Skillset")); skillset;
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
#else // ^ defined, v !defined(SEEMINACALC)
struct { char *key; u32 skillset; f32 rate; f32 target; b32 dead; } TestFiles[] = {
    { .key = "X0dd1f00da8800ee98a9ac3ebc318d4c24c5bd07d", .skillset = 1, .rate = 1.30f, .target = 34.00f  },
    { .key = "X24085a6e074ca3bd89c91b748d9b42061863e9c1", .skillset = 1, .rate = 1.00f, .target = 25.00f  },
    { .key = "X3b87b6ac44151291b29e3fe2e8b047f6af28000e", .skillset = 1, .rate = 1.00f, .target = 29.00f  },
    { .key = "X3ecb1e6f7e1334218f0f71ff0ff3bea483210f86", .skillset = 1, .rate = 1.00f, .target = 28.00f  },
    { .key = "X41972c4a5e0e20c7d33e6d4154d1e59213c805ae", .skillset = 1, .rate = 1.30f, .target = 34.00f  },
    { .key = "X554bd4927c49c94e20fa44176db5224a7dfa6244", .skillset = 1, .rate = 1.40f, .target = 35.00f  },
    { .key = "X6be11ac59dcd2ef950bb28fec8cf0b05945c93ec", .skillset = 1, .rate = 1.00f, .target = 27.00f  },
    { .key = "X777a08977f59f88273e6cbf280b0997995e1a050", .skillset = 1, .rate = 1.20f, .target = 35.00f  },
    { .key = "X8a7310367a2479daa48888b15b8be724452c4616", .skillset = 1, .rate = 1.00f, .target = 29.50f  },
    { .key = "X940b025212271d4825a986e5df80abb3e67a8097", .skillset = 1, .rate = 1.40f, .target = 35.00f  },
    { .key = "X984f26aacbe104976dfd80b002556c722163ba35", .skillset = 1, .rate = 1.00f, .target = 20.50f  },
    { .key = "Xb357a2af8046c530c54fd8dfba4ef1387908d1ca", .skillset = 1, .rate = 1.50f, .target = 34.00f  },
    { .key = "Xd0bfe128fe29964dc1d9a8c3c0fa0a98bb5a39a3", .skillset = 1, .rate = 1.20f, .target = 35.00f  },
    { .key = "Xf0fbf2b1f5f1583f7f9faadb89f149c25d2ab8f3", .skillset = 1, .rate = 1.00f, .target = 30.00f  },
    { .key = "X11698acb2346ed45032768120064f24d3e1d282d", .skillset = 2, .rate = 1.00f, .target = 23.50f  },
    { .key = "X28f9ac666d3660bdba3b815f5af87f6009db9fb6", .skillset = 2, .rate = 1.60f, .target = 35.00f  },
    { .key = "X2ed2aa7681f1f85119abae49985130bdc420e388", .skillset = 2, .rate = 1.00f, .target = 26.50f  },
    { .key = "X6a0c2fb0b567cdbc85e9eac3dbae40a6e66c7091", .skillset = 2, .rate = 1.00f, .target = 25.50f  },
    { .key = "X8f23eba0610e81e8724147c4550288be26a4f71c", .skillset = 2, .rate = 1.50f, .target = 34.50f  },
    { .key = "Xa7d67b5de8b502a9e424b398c018e6998c185614", .skillset = 2, .rate = 1.00f, .target = 29.00f  },
    { .key = "X04f71d19853f5c7d5db455178591f08215aa98e7", .skillset = 3, .rate = 1.50f, .target = 34.00f  },
    { .key = "X6ea10eba800cfcbfe462e902da3d3cdfb8d546d9", .skillset = 3, .rate = 1.45f, .target = 35.50f  },
    { .key = "X30acaf360a0e56bcc1376d36a98d24090a65074d", .skillset = 4, .rate = 1.00f, .target = 23.50f  },
    { .key = "X75012d7a17a174681beb31ce3858d1c9f726dddb", .skillset = 4, .rate = 1.00f, .target = 30.00f  },
    { .key = "X9cd21dc241d821b69967a0058ccadcedf3cc9545", .skillset = 4, .rate = 1.00f, .target = 30.50f  },
    { .key = "X07ee1f3ff7027ca945a44849afc0d6d8c09eda73", .skillset = 5, .rate = 1.40f, .target = 35.50f  },
    { .key = "X0bd2cfaff3695b86086cd972d0ff665883808fa0", .skillset = 5, .rate = 2.00f, .target = 33.00f  },
    { .key = "X1aa36f55e727dc03e463c20980400a795845bb66", .skillset = 5, .rate = 1.20f, .target = 35.50f  },
    { .key = "X2a84d2f704a13003a0b42c95ffea2fe000d5d07c", .skillset = 5, .rate = 1.60f, .target = 34.00f  },
    { .key = "X33a9827dc27dc5df9a7e91ae93d638dd0fcec022", .skillset = 5, .rate = 1.80f, .target = 32.50f  },
    { .key = "X3495c047b9e2518e22f2272224a9b46cf5124326", .skillset = 5, .rate = 1.90f, .target = 34.50f  },
    { .key = "X3f529d3cd460ac7b36547b88772eb24ddd39dc33", .skillset = 5, .rate = 1.00f, .target = 32.50f  },
    { .key = "X4d6bd3b7e752429b176f992d5df72e1c8fae1970", .skillset = 5, .rate = 1.00f, .target = 33.00f  },
    { .key = "X5a1c3429c0b87900e642405f88c3c54db0acbb3f", .skillset = 5, .rate = 1.80f, .target = 33.50f  },
    { .key = "X67747444313202c8a293779d25fe76a046d51843", .skillset = 5, .rate = 1.40f, .target = 35.50f  },
    { .key = "X680ea067b24674983f37aca016f9324ef1105810", .skillset = 5, .rate = 1.00f, .target = 35.00f  },
    { .key = "X6d5f0db56f727336fdd35494f676e049607201c1", .skillset = 5, .rate = 1.20f, .target = 35.00f  },
    { .key = "X7104e2816572025bf08344ab48e6b41addcb618b", .skillset = 5, .rate = 1.30f, .target = 34.50f  },
    { .key = "X7246b27422f4914a47b7fff57516785fe923a153", .skillset = 5, .rate = 1.20f, .target = 34.50f  },
    { .key = "X85e5db71c00c154dc2c58cf1a87f9fb5a6393b99", .skillset = 5, .rate = 1.30f, .target = 34.50f  },
    { .key = "X8644d5d21b24b7337a3793a4543f4c016e41c129", .skillset = 5, .rate = 1.80f, .target = 34.50f  },
    { .key = "X997686806022502805e5fb738a84872846c5aa24", .skillset = 5, .rate = 1.70f, .target = 32.00f  },
    { .key = "Xa07fe545c2de90d9f213a5c498216b7d940477aa", .skillset = 5, .rate = 1.00f, .target = 36.00f  },
    { .key = "Xa5a2909de3610928e4b38100542a990a483d1dcc", .skillset = 5, .rate = 1.80f, .target = 34.00f  },
    { .key = "Xa6354153c6d83a8df968ef3a23a657264a65d961", .skillset = 5, .rate = 2.00f, .target = 35.00f  },
    { .key = "Xa8f9671eeec13b573a35242d4d2e703a45ca8d35", .skillset = 5, .rate = 2.00f, .target = 32.50f  },
    { .key = "Xb12433fdc71f71392ffb61a9545914e3ca33739c", .skillset = 5, .rate = 1.60f, .target = 35.50f  },
    { .key = "Xb830d6e52c0acc2f4c655cc2062f68444e7cd231", .skillset = 5, .rate = 1.60f, .target = 35.00f  },
    { .key = "Xbbb395ecf700aafb93477de367ea485f60ae1ea0", .skillset = 5, .rate = 1.60f, .target = 35.50f  },
    { .key = "Xc35256184d06b42690eaed15feaf810432beb0f5", .skillset = 5, .rate = 1.30f, .target = 34.00f  },
    { .key = "Xcad56ae0e72fc384cd09db76b60a6b73088f16e0", .skillset = 5, .rate = 1.00f, .target = 32.00f  },
    { .key = "Xcb5737d3425bfc5c770510d9c86fafd7d3b47be0", .skillset = 5, .rate = 1.50f, .target = 34.50f  },
    { .key = "Xd94ccb09cdae9e4d66a34625af7cd8080dc00ca4", .skillset = 5, .rate = 1.40f, .target = 33.00f  },
    { .key = "Xdc3ffa63396c3609cb59fd68093e0ded7e5567be", .skillset = 5, .rate = 1.80f, .target = 34.00f  },
    { .key = "X7246b27422f4914a47b7fff57516785fe923a153", .skillset = 5, .rate = 1.00f, .target = 28.50f  },// zSong='Aztec Templing'>
    { .key = "Xd94ccb09cdae9e4d66a34625af7cd8080dc00ca4", .skillset = 5, .rate = 1.50f, .target = 35.50f  },// zSong='Need U More'>
    { .key = "X67747444313202c8a293779d25fe76a046d51843", .skillset = 5, .rate = 1.30f, .target = 33.00f  },// zSong='Fire'>
    { .key = "X67747444313202c8a293779d25fe76a046d51843", .skillset = 5, .rate = 1.30f, .target = 24.50f  },// zSong='Fire'>
    { .key = "X85e5db71c00c154dc2c58cf1a87f9fb5a6393b99", .skillset = 5, .rate = 1.00f, .target = 27.00f  },// zSong='The Vital Vitriol'>
    { .key = "Xc234a5a9a8ccb81932b7b7677ecb71cc8d1a7243", .skillset = 5, .rate = 1.00f, .target = 28.50f  },// zSong='Delta Decision'>
    { .key = "Xc35256184d06b42690eaed15feaf810432beb0f5", .skillset = 5, .rate = 1.00f, .target = 25.50f  },//zSong='zl'>
    { .key = "Xa6354153c6d83a8df968ef3a23a657264a65d961", .skillset = 5, .rate = 1.30f, .target = 24.00f  }, //zSong='OrBiTal'>"
    { .key = "Xbbb395ecf700aafb93477de367ea485f60ae1ea0", .skillset = 5, .rate = 1.00f, .target = 22.25f  }, //zSong='Infinitude Starshine'>
    // more. these aren't normative, just sledgehammers to force the optimizer away from certain outliers
    // busy? or dizzy?
    { .key = "X659797053f2679f4a8d3caaeb39455abba08cedc", .skillset = 5, .rate = 1.00f, .target = 27.00f },
    // maid of fire
    { .key = "Xb12433fdc71f71392ffb61a9545914e3ca33739c", .skillset = 5, .rate = 1.00f, .target = 23.50f },
    // mina nightmare night
    { .key = "X549a3520add30d4e5fdd4ecc64d11e99529f8564", .skillset = 5, .rate = 1.00f, .target = 24.00 },
    // brush your teeth
    { .key = "X2fc490d449b4aaf031b6910346d4f57c3d9202cc", .skillset = 5, .rate = 1.00f, .target = 24.00 },
    // princess bride
    { .key = "X02bdb23abcf2ff5b91c4110793b2923de6a75d4a", .skillset = 5, .rate = 1.60f, .target = 26.00f },
    { .key = "X32c2f335b2f746c8b45908ab31b1a9ae42d439cb", .skillset = 6, .rate = 1.20f, .target = 33.50f  },
    { .key = "X330138373e447c3c9b542f81eb4b246385766657", .skillset = 6, .rate = 1.35f, .target = 34.50f  },
    { .key = "X34dfe3ac8dc2a002b596049065629e59b967dcd6", .skillset = 6, .rate = 1.80f, .target = 34.50f  },
    { .key = "X48a3d250974b8a04e85d3ec5afc6406fdee488d7", .skillset = 6, .rate = 1.00f, .target = 35.50f  },
    { .key = "X6b297e8342727adb252b19a619e5f91520cd598e", .skillset = 6, .rate = 1.20f, .target = 31.00f  },
    { .key = "X95e507e5251b1fc7f63f2ab6ba35644c979805f5", .skillset = 6, .rate = 1.15f, .target = 33.50f  },
    { .key = "X9d2040c9346bbc212e937b40679a0daae806d6d1", .skillset = 6, .rate = 1.70f, .target = 33.00f  },
    { .key = "Xac9ea37040ca0b5c2ed251bf5206602951b474e2", .skillset = 6, .rate = 1.00f, .target = 33.00f  },
    { .key = "Xb91b0fbc02ef5c52297ec6bcfc9a6ebe423d7bde", .skillset = 6, .rate = 1.20f, .target = 32.50f  },
    { .key = "Xfeb6bdbbe84c2abfc185f9a89b464bf8b8d90c3c", .skillset = 6, .rate = 1.90f, .target = 32.50f  },
    { .key = "X5c380afe7dcf7976e77673db5afebc93cf3665de", .skillset = 6, .rate = 1.00f, .target = 34.00f  },
    { .key = "X0b685f34b530af918de78b98f3a80890df324926", .skillset = 7, .rate = 1.00f, .target = 27.00f  },
    { .key = "X3795fa69ff3b3cb8fb60e72aa72fe077cb9d6e7d", .skillset = 7, .rate = 1.40f, .target = 34.00f  },
    { .key = "X3e6a2f7e924b9b2cc784aa02c02ad2772cf77ea9", .skillset = 7, .rate = 1.00f, .target = 25.50f  },
    { .key = "X476c604a595e2d8b1c53753574b00478dfc1f4cb", .skillset = 7, .rate = 1.40f, .target = 33.00f  },
    { .key = "X4b7fae2e84e2115a552bcdb0c336ba11b0062457", .skillset = 7, .rate = 1.40f, .target = 34.50f  },
 // is a straight forward chordjack file. reinserted as the last file with skillset = 6 above.
//  { .key = "X5c380afe7dcf7976e77673db5afebc93cf3665de", .skillset = 7, .rate = 1.00f, .target = 34.00f  },
    { .key = "X65ca339f5d1ff04970562403817c06658304ae0c", .skillset = 7, .rate = 1.40f, .target = 33.00f  },
    { .key = "X6c9b70bcee689d34efdbc25aa9d02d623b018576", .skillset = 7, .rate = 1.60f, .target = 33.00f  },
    { .key = "X7071eca33dfbb4850059bb6b8718f0cf65e46168", .skillset = 7, .rate = 1.00f, .target = 20.00f  },
    { .key = "X8316aa1faf5f93f487f74ff9a10647ecd18ac0bb", .skillset = 7, .rate = 1.70f, .target = 33.50f  },
    { .key = "X87bc96672f47c365a12c7226b895d5535e5f1a60", .skillset = 7, .rate = 1.00f, .target = 20.00f  },
    { .key = "Xa97fb4d36880e81011717a25c5dbabbebfb0eb7d", .skillset = 7, .rate = 1.30f, .target = 34.00f  },
    { .key = "Xaab20a4cb1634a23ad6f077c4cb6fef755769e6e", .skillset = 7, .rate = 1.00f, .target = 17.00f  },
    { .key = "Xbe5522e563ca0e4360ae5e26b8e2cacddf943828", .skillset = 7, .rate = 1.80f, .target = 32.00f  },
    { .key = "Xe12f18d0d35797878b7aa1d0624e7bb9c15bff6f", .skillset = 7, .rate = 1.70f, .target = 34.00f  },
    { .key = "Xe4e68826a3d00062e163f3a29b42350f86089b26", .skillset = 7, .rate = 1.00f, .target = 25.00f  },
    { .key = "Xecbb9d26ff2e40424d3c09e2bfe405149b954e2b", .skillset = 7, .rate = 1.00f, .target = 22.50f  },
    { .key = "Xfc8c31f8022a6bd24fe186ea381917b697ea14a2", .skillset = 7, .rate = 1.40f, .target = 33.50f  },
    { .key = "Xdd74f464e7acbab921b91a2b5100901af24b3054", .skillset = 4, .rate = 1.00f, .target = 25.50f, .dead = 1  },
    { .key = "X02bdb23abcf2ff5b91c4110793b2923de6a75d4a", .skillset = 5, .rate = 2.00f, .target = 36.00f, .dead = 1  },
    { .key = "X04241934a67a881dce270b9a59c12d7d3465707d", .skillset = 5, .rate = 1.80f, .target = 33.50f, .dead = 1  },
 // chartkey points at medium diff, which is not a jackspeed file (the hard diff is)
//  { .key = "X0b9f174c0842900f11f319ef92ebc29a8d136c40", .skillset = 5, .rate = 1.80f, .target = 31.50f, .dead = 1  },
    { .key = "X0bd2cfaff3695b86086cd972d0ff665883808fa0", .skillset = 5, .rate = 1.80f, .target = 31.50f, .dead = 1  },
    { .key = "X228dad35261a8c058c3559e51795e0d57d1e73b4", .skillset = 5, .rate = 1.50f, .target = 32.00f, .dead = 1  },
    { .key = "X3495c047b9e2518e22f2272224a9b46cf5124326", .skillset = 5, .rate = 1.70f, .target = 33.00f, .dead = 1  },
    { .key = "X384726c68bdc0c7267740c9398809b26ef7b253b", .skillset = 5, .rate = 1.70f, .target = 33.50f, .dead = 1  },
    { .key = "X39c8da1316785aa5b41f5fcae3344d09aa32cbba", .skillset = 5, .rate = 1.90f, .target = 34.00f, .dead = 1  },
    { .key = "X42dfdf8d468f14893e4fbbc7c38e1d84ed1f65b5", .skillset = 5, .rate = 1.50f, .target = 33.50f, .dead = 1  },
    { .key = "X46fe26ac44ce680907eef143d4027e43e47a385c", .skillset = 5, .rate = 1.70f, .target = 34.00f, .dead = 1  },
    { .key = "X4d6bd3b7e752429b176f992d5df72e1c8fae1970", .skillset = 5, .rate = 1.00f, .target = 32.50f, .dead = 1  },
    { .key = "X680ea067b24674983f37aca016f9324ef1105810", .skillset = 5, .rate = 1.00f, .target = 35.00f, .dead = 1  },
    { .key = "X6e20373704afadfc43eebf69217f986ad4ada494", .skillset = 5, .rate = 1.70f, .target = 33.50f, .dead = 1  },
    { .key = "X7246b27422f4914a47b7fff57516785fe923a153", .skillset = 5, .rate = 1.20f, .target = 34.00f, .dead = 1  },
    { .key = "X7b294e038ba6c4cf1fc80de0eff191f928c50080", .skillset = 5, .rate = 2.00f, .target = 34.00f, .dead = 1  },
    { .key = "X85e5db71c00c154dc2c58cf1a87f9fb5a6393b99", .skillset = 5, .rate = 1.00f, .target = 26.00f, .dead = 1  },
    { .key = "X946eebfd26510999d1092bba4e4ebee117b88d6f", .skillset = 5, .rate = 1.60f, .target = 33.50f, .dead = 1  },
    { .key = "Xa07fe545c2de90d9f213a5c498216b7d940477aa", .skillset = 5, .rate = 1.00f, .target = 35.00f, .dead = 1  },
    { .key = "Xa5a2909de3610928e4b38100542a990a483d1dcc", .skillset = 5, .rate = 1.80f, .target = 33.50f, .dead = 1  },
    { .key = "Xa6354153c6d83a8df968ef3a23a657264a65d961", .skillset = 5, .rate = 2.00f, .target = 34.00f, .dead = 1  },
    { .key = "Xa8f9671eeec13b573a35242d4d2e703a45ca8d35", .skillset = 5, .rate = 2.00f, .target = 32.50f, .dead = 1  },
    { .key = "Xae07974ee0b0a60b3b5c7d554859cc336e0fb296", .skillset = 5, .rate = 1.70f, .target = 33.00f, .dead = 1  },
    { .key = "Xb11b2b8b93d2f3e2964e2872515889feb1250dd9", .skillset = 5, .rate = 1.60f, .target = 32.50f, .dead = 1  },
    { .key = "Xb12433fdc71f71392ffb61a9545914e3ca33739c", .skillset = 5, .rate = 1.50f, .target = 32.50f, .dead = 1  },
    { .key = "Xb2d874e23ab11202217f6e879dae662a84cf293f", .skillset = 5, .rate = 2.00f, .target = 30.50f, .dead = 1  },
    { .key = "Xb830d6e52c0acc2f4c655cc2062f68444e7cd231", .skillset = 5, .rate = 1.60f, .target = 33.50f, .dead = 1  },
    { .key = "Xc35256184d06b42690eaed15feaf810432beb0f5", .skillset = 5, .rate = 1.30f, .target = 34.00f, .dead = 1  },
    { .key = "Xcee78c72ba6ba436ba9325f030c482bf13843376", .skillset = 5, .rate = 1.00f, .target = 17.50f, .dead = 1  },
    { .key = "Xd94ccb09cdae9e4d66a34625af7cd8080dc00ca4", .skillset = 5, .rate = 1.40f, .target = 33.00f, .dead = 1  },
    { .key = "Xdc3ffa63396c3609cb59fd68093e0ded7e5567be", .skillset = 5, .rate = 1.80f, .target = 33.50f, .dead = 1  },
    { .key = "Xdf0158c52b5a3fd5ecb2cf8b65999af06020bd34", .skillset = 5, .rate = 1.50f, .target = 32.00f, .dead = 1  },
    { .key = "Xe0a7e1a1d4f4584db3dac036c1e6df30231388b8", .skillset = 5, .rate = 2.00f, .target = 33.50f, .dead = 1  },
    { .key = "Xf75a6185cf41fb74e95bb1c8d2655c5bf9b39cda", .skillset = 5, .rate = 1.70f, .target = 33.50f, .dead = 1  },
    { .key = "Xf96c0c2d42c86624a6e80013623042d4b1bce323", .skillset = 5, .rate = 1.10f, .target = 32.50f, .dead = 1  },
};

int main(int argc, char **argv)
{
    assert(argc == 2);

    isize bignumber = 100*1024*1024;
    scratch_stack = stack_make(malloc(bignumber), bignumber);
    permanent_memory_stack = stack_make(malloc(bignumber), bignumber);

    u8 *gen = 0;
    buf_printf(gen,
        "// File generated by cachedb.c\n\n"
        "typedef struct TargetFile\n"
        "{\n"
        "    String key;\n"
        "    String author;\n"
        "    String title;\n"
        "    i32 difficulty;\n"
        "    i32 skillset;\n"
        "    f32 rate;\n"
        "    f32 target;\n"
        "    f32 weight;\n"
        "    isize note_data_len;\n"
        "    u32 *note_data_notes;\n"
        "    f32 *note_data_times;\n"
        "} TargetFile;\n\n"
    );

    DBFile files[array_length(TestFiles)] = {0};
    u8 **missing = 0;

    db_init(argv[1]);
    for (isize i = 0; i < array_length(TestFiles); i++) {
        DBFile dbf = db_get_file(TestFiles[i].key);
        if (dbf.ok) {
            NoteInfo *nd = (NoteInfo *)dbf.note_data.buf;
            isize nd_len = dbf.note_data.len / sizeof(NoteInfo);

            buf_printf(gen, "i32 TargetFileData_%zd_Notes[] = { ", i);
            for (isize index = 0; index < nd_len; index++) {
                if ((index & 31) == 0) {
                    buf_printf(gen, "\n    ");
                }
                buf_printf(gen, "%2d,", nd[index].notes);
            }
            buf_pop(gen);
            buf_printf(gen, "\n};\n\n");

            buf_printf(gen, "f32 TargetFileData_%zd_RowTime[] = { ", i);
            for (isize index = 0; index < nd_len; index++) {
                if ((index & 7) == 0) {
                    buf_printf(gen, "\n    ");
                }
                buf_printf(gen, "%a,", nd[index].rowTime);
            }
            buf_pop(gen);
            buf_printf(gen, "\n};\n\n");
        } else {
            buf_push(missing, TestFiles[i].key);
        }

        files[i] = dbf;
    }

    buf_printf(gen, "// Missing files:\n");
    for (isize i = 0; i < buf_len(missing); i++) {
        buf_printf(gen, "//  %s\n", missing[i]);
    }
    buf_printf(gen, "\n");

    buf_printf(gen, "TargetFile TargetFiles[] = {\n");
    for (isize i = 0; i < array_length(TestFiles); i++) {
        if (files[i].ok) {
            buf_printf(gen, "    {\n");
            buf_printf(gen, "        .key = SS(\"%s\"),\n", files[i].chartkey.buf);
            buf_printf(gen, "        .author = SS(\"%s\"),\n", files[i].author.buf);
            buf_printf(gen, "        .title = SS(\"%s\"),\n", files[i].title.buf);
            buf_printf(gen, "        .difficulty = %d,\n", files[i].difficulty);
            buf_printf(gen, "        .skillset = %d,\n", TestFiles[i].skillset);
            buf_printf(gen, "        .rate = %af,\n", TestFiles[i].rate);
            buf_printf(gen, "        .target = %af,\n", TestFiles[i].target);
            buf_printf(gen, "        .weight = %af,\n", TestFiles[i].dead ? 0.0f : 1.0f);
            buf_printf(gen, "        .note_data_len = array_length(TargetFileData_%zd_Notes),\n", i);
            buf_printf(gen, "        .note_data_notes = TargetFileData_%zd_Notes,\n", i);
            buf_printf(gen, "        .note_data_times = TargetFileData_%zd_RowTime\n", i);
            buf_printf(gen, "    },\n");
        }
    }
    buf_printf(gen, "};\n\n");
    buf_printf(gen, "static const isize NumTargetFiles = array_length(TargetFiles);\n");

    write_file("cachedb.gen.c", gen);
}
#endif
