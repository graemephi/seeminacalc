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

#include "sqlite3.c"
// The perils of including sqlite3 in your translation unit
// sqlite3.c also disables certain warnings.
#if !defined(RELEASE)
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

struct { char *key; u32 skillset; f32 rate; f32 target; } TestFiles[] = {
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
    { .key = "Xdd74f464e7acbab921b91a2b5100901af24b3054", .skillset = 4, .rate = 1.00f, .target = 25.50f  },
    { .key = "X02bdb23abcf2ff5b91c4110793b2923de6a75d4a", .skillset = 5, .rate = 2.00f, .target = 36.00f  },
    { .key = "X04241934a67a881dce270b9a59c12d7d3465707d", .skillset = 5, .rate = 1.80f, .target = 33.50f  },
 // chartkey points at medium diff, which is not a jackspeed file (the hard diff is)
//  { .key = "X0b9f174c0842900f11f319ef92ebc29a8d136c40", .skillset = 5, .rate = 1.80f, .target = 31.50f  },
    { .key = "X0bd2cfaff3695b86086cd972d0ff665883808fa0", .skillset = 5, .rate = 1.80f, .target = 31.50f  },
    { .key = "X228dad35261a8c058c3559e51795e0d57d1e73b4", .skillset = 5, .rate = 1.50f, .target = 32.00f  },
    { .key = "X3495c047b9e2518e22f2272224a9b46cf5124326", .skillset = 5, .rate = 1.70f, .target = 33.00f  },
    { .key = "X384726c68bdc0c7267740c9398809b26ef7b253b", .skillset = 5, .rate = 1.70f, .target = 33.50f  },
    { .key = "X39c8da1316785aa5b41f5fcae3344d09aa32cbba", .skillset = 5, .rate = 1.90f, .target = 34.00f  },
    { .key = "X42dfdf8d468f14893e4fbbc7c38e1d84ed1f65b5", .skillset = 5, .rate = 1.50f, .target = 33.50f  },
    { .key = "X46fe26ac44ce680907eef143d4027e43e47a385c", .skillset = 5, .rate = 1.70f, .target = 34.00f  },
    { .key = "X4d6bd3b7e752429b176f992d5df72e1c8fae1970", .skillset = 5, .rate = 1.00f, .target = 32.50f  },
    { .key = "X680ea067b24674983f37aca016f9324ef1105810", .skillset = 5, .rate = 1.00f, .target = 35.00f  },
    { .key = "X6e20373704afadfc43eebf69217f986ad4ada494", .skillset = 5, .rate = 1.70f, .target = 33.50f  },
    { .key = "X7246b27422f4914a47b7fff57516785fe923a153", .skillset = 5, .rate = 1.20f, .target = 34.00f  },
    { .key = "X7b294e038ba6c4cf1fc80de0eff191f928c50080", .skillset = 5, .rate = 2.00f, .target = 34.00f  },
    { .key = "X85e5db71c00c154dc2c58cf1a87f9fb5a6393b99", .skillset = 5, .rate = 1.00f, .target = 26.00f  },
    { .key = "X946eebfd26510999d1092bba4e4ebee117b88d6f", .skillset = 5, .rate = 1.60f, .target = 33.50f  },
    { .key = "Xa07fe545c2de90d9f213a5c498216b7d940477aa", .skillset = 5, .rate = 1.00f, .target = 35.00f  },
    { .key = "Xa5a2909de3610928e4b38100542a990a483d1dcc", .skillset = 5, .rate = 1.80f, .target = 33.50f  },
    { .key = "Xa6354153c6d83a8df968ef3a23a657264a65d961", .skillset = 5, .rate = 2.00f, .target = 34.00f  },
    { .key = "Xa8f9671eeec13b573a35242d4d2e703a45ca8d35", .skillset = 5, .rate = 2.00f, .target = 32.50f  },
    { .key = "Xae07974ee0b0a60b3b5c7d554859cc336e0fb296", .skillset = 5, .rate = 1.70f, .target = 33.00f  },
    { .key = "Xb11b2b8b93d2f3e2964e2872515889feb1250dd9", .skillset = 5, .rate = 1.60f, .target = 32.50f  },
    { .key = "Xb12433fdc71f71392ffb61a9545914e3ca33739c", .skillset = 5, .rate = 1.50f, .target = 32.50f  },
    { .key = "Xb2d874e23ab11202217f6e879dae662a84cf293f", .skillset = 5, .rate = 2.00f, .target = 30.50f  },
    { .key = "Xb830d6e52c0acc2f4c655cc2062f68444e7cd231", .skillset = 5, .rate = 1.60f, .target = 33.50f  },
    { .key = "Xc35256184d06b42690eaed15feaf810432beb0f5", .skillset = 5, .rate = 1.30f, .target = 34.00f  },
    { .key = "Xcee78c72ba6ba436ba9325f030c482bf13843376", .skillset = 5, .rate = 1.00f, .target = 17.50f  },
    { .key = "Xd94ccb09cdae9e4d66a34625af7cd8080dc00ca4", .skillset = 5, .rate = 1.40f, .target = 33.00f  },
    { .key = "Xdc3ffa63396c3609cb59fd68093e0ded7e5567be", .skillset = 5, .rate = 1.80f, .target = 33.50f  },
    { .key = "Xdf0158c52b5a3fd5ecb2cf8b65999af06020bd34", .skillset = 5, .rate = 1.50f, .target = 32.00f  },
    { .key = "Xe0a7e1a1d4f4584db3dac036c1e6df30231388b8", .skillset = 5, .rate = 2.00f, .target = 33.50f  },
    { .key = "Xf75a6185cf41fb74e95bb1c8d2655c5bf9b39cda", .skillset = 5, .rate = 1.70f, .target = 33.50f  },
    { .key = "Xf96c0c2d42c86624a6e80013623042d4b1bce323", .skillset = 5, .rate = 1.10f, .target = 32.50f  },
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

    // more. these aren't normative, just sledgehammers to force the optimizer away from certain outliers
    // busy? or dizzy?
    { .key = "X659797053f2679f4a8d3caaeb39455abba08cedc", .skillset = 5, .rate = 1.00f, .target = 27.00f  },
    // maid of fire
    { .key = "Xb12433fdc71f71392ffb61a9545914e3ca33739c", .skillset = 5, .rate = 1.00f, .target = 23.50f  },
    // mina nightmare night
    { .key = "X549a3520add30d4e5fdd4ecc64d11e99529f8564", .skillset = 5, .rate = 1.00f, .target = 24.00f },
    // brush your teeth
    { .key = "X2fc490d449b4aaf031b6910346d4f57c3d9202cc", .skillset = 5, .rate = 1.00f, .target = 24.00f },
    // princess bride
    { .key = "X02bdb23abcf2ff5b91c4110793b2923de6a75d4a", .skillset = 5, .rate = 1.60f, .target = 26.00f  },
    // #er
    { .key = "X585a0b45879134590c62c294ab12f611bcd242b2", .skillset = 5, .rate = 1.0f, .target = 27.0f },
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
