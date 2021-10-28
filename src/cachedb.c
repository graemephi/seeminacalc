#ifdef __clang__
// Disabling these warnings is for libraries only.
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

#if defined(__EMSCRIPTEN__)
// One weird trick to avoid sqlite pulling in the emscripten FS library
void utimes(void *a, int b) {}
#endif

// Extremely jank xml reader
typedef struct {
    b32 ok;
    u8 *ptr;
    u8 *end;
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
            if (p + 1 < end && p[1] == '!') {
                // <!-- comment -->
                while ((p+2) < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) {
                    p++;
                }
            } else {
                break;
            }
        }
        x->ok = p < end;
        x->ptr = p;
    }
    return x->ok;
}

String xml_token(u8 *p)
{
    String result = { .buf = p };
    while (*p && !isspace(*p) && *p != '<' && *p != '>') {
        p++;
    }
    result.len = p - (u8 *)result.buf;
    return result;
}

b32 xml_open(XML *x, String tag)
{
    if (x->ok) {
        do {
            if (buf_len(x->stack) >= 32) {
                x->ok = false;
                return false;
            }
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
                x->ok = (*p == '>');
                return x->ok;
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

b32 scan_to_char(u8 **pp, u8 *end, u8 c)
{
    u8 *p = *pp;
    while (*p && p != end && *p != c) {
        p++;
    }
    *pp = p;
    return *p == c;
}

b32 scan_to_char_skip_whitespace(u8 **pp, u8 *end, u8 c)
{
    u8 *p = *pp;
    while (*p && p != end && (*p != c || *p <= ' ')) {
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
        if (!scan_to_char_skip_whitespace(&p, end, '=')) {
            goto bail;
        }
        if (!scan_to_char_skip_whitespace(&p, end, '\'')) {
            goto bail;
        }
        p++;
        u8 *start_of_value = p;
        if (!scan_to_char(&p, end, '\'')) {
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

typedef struct DBFile
{
    b32 ok;
    i32 difficulty;
    String title;
    String subtitle;
    String artist;
    String author;
    String chartkey;
    String all_msds;
    String bpms;
    f32 first_second;
    i32 skillset;
    f32 rating;
    NoteData *note_data;
    usize n_rows;
} DBFile;

void dbfile_set_skillset_and_rating_from_all_msds(DBFile *f)
{
    if (f->skillset != 0) {
        return;
    }

    u8 const *p = f->all_msds.buf;
    u8 *end = f->all_msds.buf + f->all_msds.len;
    for (isize i = 0; i < 3; i++) {
        while (p < end && *p && *p++ != ':') {
            ;
        }
    }
    if (ALWAYS(p < end)) {
        float msd[NumSkillsets] = {0};
        sscanf(p, "%f,%f,%f,%f,%f,%f,%f,%f",
            msd + 0,
            msd + 1,
            msd + 2,
            msd + 3,
            msd + 4,
            msd + 5,
            msd + 6,
            msd + 7
        );
        float top = msd[1];
        i32 ss = 1;
        for (i32 i = 2; i < NumSkillsets; i++) {
            if (msd[i] > top) {
                top = msd[i];
                ss = i;
            }
        }
        f->skillset = ss;
        f->rating = msd[0];
    }
}

enum {
    DBRequestQueueSize = 64,
    DBRequestQueueCapacity = DBRequestQueueSize - 1,
    DBRequestQueueMask = DBRequestQueueSize - 1,
    DBResultQueueSize = 4096,
    DBResultQueueCapacity = DBResultQueueSize - 1,
    DBResultQueueMask = DBResultQueueSize - 1
};

static_assert((DBRequestQueueSize & (DBRequestQueueSize - 1)) == 0);
static_assert((DBResultQueueSize & (DBResultQueueSize - 1)) == 0);

typedef enum DBRequestType
{
    DBRequest_UseDB,
    DBRequest_File,
    DBRequest_Search,
    DBRequestTypeCount
} DBRequestType;

typedef struct DBRequest
{
    DBRequestType type;
    u64 id;
    union {
        Buffer db;
        String query;
        ChartKey key;
    };
} DBRequest;

typedef struct DBResult
{
    DBRequestType type;
    String query;
    u64 id;
    DBFile file;
} DBResult;

typedef struct DBResultsByType
{
    DBResult *of[DBRequestTypeCount];
} DBResultsByType;

typedef struct DBRequestQueue
{
    alignas(64) volatile usize read;
    alignas(64) volatile usize write;
    DBRequest entries[DBRequestQueueSize];
} DBRequestQueue;

typedef struct DBResultQueue
{
    alignas(64) volatile usize read;
    alignas(64) volatile usize write;
    DBResult entries[DBResultQueueSize];
} DBResultQueue;

alignas(64) volatile u64 db_last_search_id = 0;
DBRequestQueue db_request_queue = {0};
DBResultQueue db_result_queue = {0};
BadSem db_request_sem = {0};
BadSem db_result_sem = {0};
u8 query_buffer[4096] = {0};
Stack query_stack = {0};
DBRequest *db_pending_requests = 0;
u64 db_request_id = 1;

u64 db_next_id(void)
{
    return db_request_id++;
}

void db_init(Buffer db)
{
    db_request_sem = sem_create();
    db_result_sem = sem_create();
    query_stack = stack_make(query_buffer, sizeof(query_buffer));
    buf_reserve(db_pending_requests, 1024);
    Buffer *eh = 0;
    buf_push(eh, db);
    int db_thread(void*);
    make_thread(db_thread, eh);
}

b32 db_ready(void)
{
    return db_pending_requests != 0;
}

void db_use(Buffer db)
{
    if (!db_ready()) {
        db_init(db);
    } else {
        buf_push(db_pending_requests, (DBRequest) {
            .type = DBRequest_UseDB,
            .db = db
        });
    }
}

u64 db_get(ChartKey key)
{
    u64 id = db_next_id();
    buf_push(db_pending_requests, (DBRequest) {
        .type = DBRequest_File,
        .id = id,
        .key = key
    });
    return id;
}

u64 db_search(String query)
{
    push_allocator(&query_stack);
    assert(query.len < 512);
    if (stack_space_remaining(&query_stack) < query.len) {
        stack_reset(&query_stack);
    }
    query = copy_string(query);
    pop_allocator();
    u64 id = db_next_id();
    db_last_search_id = id;

    DBRequest req = (DBRequest) {
        .type = DBRequest_Search,
        .id = id,
        .query = query
    };

    // Part of the idea was to funnel everything thru db_pump but doing sending
    // and receiving from one place results in one (1) frame of lag which turned
    // out make the search Less Nice. So we immediately push and notify, now,
    // rather than wait.
    usize read = db_request_queue.read;
    usize write = db_request_queue.write;
    memory_barrier();
    usize queue_capacity = DBRequestQueueCapacity - (write - read);
    if (queue_capacity > 0) {
        db_request_queue.entries[write & DBRequestQueueMask] = req;
        memory_barrier();
        db_request_queue.write = write + 1;
        thread_notify(db_request_sem);
    } else {
        buf_push(db_pending_requests, req);
    }

    return id;
}

DBResultsByType db_pump(void)
{
    // Submit requests
    {
        usize read = db_request_queue.read;
        usize write = db_request_queue.write;
        memory_barrier();
        usize queue_capacity = DBRequestQueueCapacity - (write - read);
        usize n = mins(queue_capacity, buf_len(db_pending_requests));
        if (n > 0) {
            for (usize i = 0; i < n; i++) {
                db_request_queue.entries[(write + i) & DBRequestQueueMask] = db_pending_requests[i];
            }
            memory_barrier();
            db_request_queue.write = write + n;
            buf_remove_first_n(db_pending_requests, n);
            thread_notify(db_request_sem);
        }
    }

    // Read results
    DBResultsByType results = {0};
    push_allocator(scratch);
    {
        usize read = db_result_queue.read;
        usize write = db_result_queue.write;
        memory_barrier();
        usize n = write - read;
        if (n > 0) {
            for (usize i = 0; i < n; i++) {
                DBResult r = db_result_queue.entries[(read + i) & DBResultQueueMask];
                buf_push(results.of[r.type], r);
            }
            memory_barrier();
            db_result_queue.read = read + n;
            thread_notify(db_result_sem);
        }
    }
    pop_allocator();

    return results;
}

b32 db_next(DBRequest *req)
{
    if (db_request_queue.read == db_request_queue.write) {
        thread_wait(db_request_sem);
    }
    usize read = db_request_queue.read;
    memory_barrier();
    *req = db_request_queue.entries[read & DBRequestQueueMask];
    memory_barrier();
    db_request_queue.read = read + 1;
    return true;
}

void db_respond(DBResult *result)
{
    while (db_result_queue.write == db_result_queue.read + (DBResultQueueSize - 1)) {
        thread_wait(db_result_sem);
    }
    usize write = db_result_queue.write;
    memory_barrier();
    db_result_queue.entries[write & DBResultQueueMask] = *result;
    memory_barrier();
    db_result_queue.write = write + 1;
}

b32 dbfile_from_stmt(sqlite3_stmt *stmt, DBFile *out)
{
    memset(out, 0, sizeof(DBFile));
    int step = sqlite3_step(stmt);
    int stmt_can_continue = (step == SQLITE_ROW);

    u8 const *title = sqlite3_column_text(stmt, 0);
    isize title_len = sqlite3_column_bytes(stmt, 0);

    u8 const *subtitle = sqlite3_column_text(stmt, 1);
    isize subtitle_len = sqlite3_column_bytes(stmt, 1);

    u8 const *artist = sqlite3_column_blob(stmt, 2);
    isize artist_len = sqlite3_column_bytes(stmt, 2);

    u8 const *author = sqlite3_column_text(stmt, 3);
    isize author_len = sqlite3_column_bytes(stmt, 3);

    int difficulty = sqlite3_column_int(stmt, 4);

    u8 const *msd = sqlite3_column_text(stmt, 5);
    isize msd_len = sqlite3_column_bytes(stmt, 5);

    u8 const *ck = sqlite3_column_blob(stmt, 6);
    isize ck_len = sqlite3_column_bytes(stmt, 6);

    void const *nd = sqlite3_column_blob(stmt, 7);
    isize nd_len = sqlite3_column_bytes(stmt, 7);

    u8 const *bpms = sqlite3_column_blob(stmt, 8);
    isize bpms_len = sqlite3_column_bytes(stmt, 8);

    f64 first_second = sqlite3_column_double(stmt, 9);

    out->difficulty = difficulty;
    out->title.len = buf_printf(out->title.buf, "%.*s", title_len, title);
    out->subtitle.len = buf_printf(out->subtitle.buf, "%.*s", subtitle_len, subtitle);
    out->artist.len = buf_printf(out->artist.buf, "%.*s", artist_len, artist);
    out->chartkey.len = buf_printf(out->chartkey.buf, "%.*s", ck_len, ck);
    out->author.len = buf_printf(out->author.buf, "%.*s", author_len, author);
    out->all_msds.len = buf_printf(out->all_msds.buf, "%.*s", msd_len, msd);
    if (nd_len > 0) {
        out->note_data = frobble_serialized_note_data(nd, nd_len);
        out->n_rows = note_data_row_count(out->note_data);
        out->bpms.len = buf_printf(out->bpms.buf, "%.*s", bpms_len, bpms);
        out->first_second = (f32)first_second;
    }
    out->ok = (ck_len > 0);
    return stmt_can_continue;
}

i32 db_thread(void *userdata)
{
    Buffer db_buffer = *(Buffer *)userdata;

    isize mb = 64*1024*1024;
    Stack file_stack = stack_make(malloc(mb), mb);
    Stack search_stack = stack_make(malloc(mb), mb);

    sqlite3 *db = 0;
    sqlite3_stmt *file_stmt = 0;
    sqlite3_stmt *search_stmt = 0;

    int rc = sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READONLY, 0);
    if (rc) {
        goto err;
    }

    rc = sqlite3_deserialize(db, "main", db_buffer.buf, db_buffer.len, db_buffer.len, SQLITE_DESERIALIZE_READONLY);
    if (rc) {
        goto err;
    }

    static const char file_query[] =
        "select songs.title, songs.title, songs.artist, songs.credit, steps.difficulty, steps.msd, steps.chartkey, steps.serializednotedata, songs.bpms, songs.offset "
        "from steps inner join songs on songs.id = steps.songid "
        "where chartkey=? and stepstype='dance-single'";
    static const char search_query[] = "select songs.title, songs.subtitle, songs.artist, songs.credit, steps.difficulty, steps.msd, steps.chartkey "
        "from steps inner join songs on songs.id = steps.songid "
        "where chartkey=?1 or instr(lower(songs.title), ?1) or instr(lower(songs.subtitle), ?1) or instr(lower(songs.titletranslit), ?1) or instr(lower(songs.credit), ?1) or instr(lower(songs.artist), ?1) and stepstype='dance-single';";

    rc = sqlite3_prepare_v2(db, file_query, sizeof(file_query), &file_stmt, 0);
    if (rc) {
        goto err;
    }

    rc = sqlite3_prepare_v2(db, search_query, sizeof(search_query), &search_stmt, 0);
    if (rc) {
        goto err;
    }

    DBRequest req = {0};
    while (db_next(&req)) {
        switch (req.type) {
            case DBRequest_UseDB: {
                free(db_buffer.buf);
                db_buffer = req.db;
                rc = sqlite3_deserialize(db, "main", db_buffer.buf, db_buffer.len, db_buffer.len, SQLITE_DESERIALIZE_READONLY);
                if (rc) {
                    goto err;
                }
            } break;
            case DBRequest_File: {
                current_allocator = &file_stack;
                String query = chartkey_to_string(req.key);
                sqlite3_bind_text(file_stmt, 1, query.buf, query.len, 0);

                DBFile file = {0};
                dbfile_from_stmt(file_stmt, &file);

                db_respond(&(DBResult) {
                    .type = req.type,
                    .id = req.id,
                    .query = query,
                    .file = file
                });

                sqlite3_reset(file_stmt);
            } break;
            case DBRequest_Search: {
                if (req.id >= db_last_search_id) {
                    current_allocator = &search_stack;
                    stack_reset(&search_stack);

                    for (isize i = 0; i < req.query.len; i++) {
                        req.query.buf[i] = (req.query.buf[i] >= 'A' && req.query.buf[i] <= 'Z') ? (req.query.buf[i] + ('a' - 'A')) : req.query.buf[i];
                    }

                    sqlite3_bind_text(search_stmt, 1, req.query.buf, req.query.len, 0);

                    DBFile file = {0};
                    while (dbfile_from_stmt(search_stmt, &file)) {
                        db_respond(&(DBResult) {
                            .type = req.type,
                            .id = req.id,
                            .query = req.query,
                            .file = file
                        });
                    }

                    sqlite3_reset(search_stmt);
                }
            } break;
            default: assert_unreachable();
        }
    }

    return 0;

err:
    fprintf(stderr, "sqlite3 oops: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
}
