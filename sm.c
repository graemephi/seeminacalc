static String sm_tag_inplace(SmFile *sm, SmTag t)
{
    SmString s = sm->tags[t];
    return (String) {
        .buf = &sm->sm.buf[s.index],
        .len = s.len
    };
}

static String sm_tag_copy(SmFile *sm, SmTag t)
{
    return copy_string(sm_tag_inplace(sm, t));
}

static void die(SmParser *ctx, char *expected)
{
    push_allocator(scratch);
    i32 current_line = 1;
    if (ctx->p) {
        for (char *p = ctx->p; p != ctx->buf; p--) {
            if (*p == '\n') {
                current_line++;
            }
        }
    }
    ctx->error = (String) {0};
    ctx->error.len = buf_printf(ctx->error.buf, "line %d: parse error. expected %s, got %c", current_line, expected, *ctx->p);
    pop_allocator();
    longjmp(*ctx->env, -1);
}

static void validate(SmParser *ctx, b32 condition, char *message)
{
    if (!condition) {
        push_allocator(scratch);
        ctx->error = (String) {0};
        if (*ctx->p && ctx->p < ctx->end) {
            i32 current_line = 1;
            for (char *p = ctx->p; p != ctx->buf; p--) {
                if (*p == '\n') {
                    current_line++;
                }
            }
            ctx->error.len = buf_printf(ctx->error.buf, "line %d: %s", current_line, message);
        } else {
            ctx->error.len = buf_printf(ctx->error.buf, "%s", message);
        }
        pop_allocator();
        longjmp(*ctx->env, -2);
    }
}

static i32 parser_position(SmParser *ctx)
{
    return (i32)(ctx->p - ctx->buf);
}

static b32 string_matches(String a, char *p)
{
    return strncmp(a.buf, p, a.len) == 0;
}

static void advance_to(SmParser *ctx, char c)
{
    char *p = strchr(ctx->p, c);
    ctx->p = p ? p : ctx->end;
}

static void consume_whitespace(SmParser *ctx)
{
    static const char whitespace[256] = {
        [' '] = 1,
        ['\r'] = 1,
        ['\n'] = 1,
        ['\t'] = 1,
        ['\f'] = 1
    };

    char *p = ctx->p;
    while (whitespace[(u8)*p] && p < ctx->end) {
        p++;
    }
    ctx->p = p;
}

static b32 try_consume_comment(SmParser *ctx)
{
    b32 result = false;
    if (ctx->p[0] == '/' && ctx->p[1] == '/') {
        advance_to(ctx, '\n');
        result = true;
    }
    return result;
}

static void consume_whitespace_and_comments(SmParser *ctx)
{
    do {
        consume_whitespace(ctx);
    } while (try_consume_comment(ctx));
}

static b32 try_consume_string(SmParser *ctx, String s)
{
    b32 result = false;
    if (string_matches(s, ctx->p)) {
        ctx->p += s.len;
        result = true;

        consume_whitespace_and_comments(ctx);
    }
    return result;
}

static void consume_string(SmParser *ctx, String s)
{
    b32 ok = try_consume_string(ctx, s);
    if (ok == false) {
        die(ctx, (char[]){ s.buf[0], 0 });
    }
}

static b32 try_consume_char(SmParser *ctx, char c)
{
    b32 result = false;

    if (*ctx->p == c) {
        ctx->p++;
        result = true;

        consume_whitespace_and_comments(ctx);
    }

    return result;
}

static void consume_char(SmParser *ctx, char c)
{
    b32 ok = try_consume_char(ctx, c);
    if (ok == false) {
        die(ctx, (char[]){c, 0});
    }
}

static f32 parse_f32(SmParser *ctx)
{
    char *p = ctx->p;
    f32 result = strtof(p, &p);
    if (p == ctx->p) {
        die(ctx, "number");
    }
    ctx->p = p;
    consume_whitespace_and_comments(ctx);
    return result;
}

static const u8 SmFileNote[256] = {
    ['0'] = Note_Off,
    ['1'] = Note_On,
    ['2'] = Note_HoldStart,
    ['3'] = Note_HoldEnd,
    ['4'] = Note_RollStart,
    ['M'] = Note_Mine,
    ['K'] = Note_Off,
    ['L'] = Note_Lift,
    ['F'] = Note_Fake,
};

static const u8 SmFileNoteValid[256] = {
    ['0'] = true,
    ['1'] = true,
    ['2'] = true,
    ['3'] = true,
    ['4'] = true,
    ['M'] = true,
    ['K'] = true,
    ['L'] = true,
    ['F'] = true,
};

static const String DifficultyStrings[DiffCount] = {
    SS("Beginner"),
    SS("Easy"),
    SS("Medium"),
    SS("Hard"),
    SS("Challenge"),
    SS("Edit"),
};

static const String TagStrings[TagCount] = {
    SS("ARTISTTRANSLIT"),
    SS("ARTIST"),
    SS("ATTACKS"),
    SS("BACKGROUND"),
    SS("BANNER"),
    SS("BGCHANGES"),
    SS("BPMS"),
    SS("CDTITLE"),
    SS("CREDIT"),
    SS("DELAYS"),
    SS("DISPLAYBPM"),
    SS("FGCHANGES"),
    SS("GENRE"),
    SS("KEYSOUNDS"),
    SS("LYRICSPATH"),
    SS("MUSIC"),
    SS("NOTES"),
    SS("OFFSET"),
    SS("SAMPLELENGTH"),
    SS("SAMPLESTART"),
    SS("SELECTABLE"),
    SS("STOPS"),
    SS("SUBTITLETRANSLIT"),
    SS("SUBTITLE"),
    SS("TIMESIGNATURES"),
    SS("TITLETRANSLIT"),
    SS("TITLE"),
};

static SmTagValue parse_tag(SmParser *ctx)
{
    consume_char(ctx, '#');
    SmTag tag = TagCount;
    for (isize i = 0; i < TagCount; i++) {
        if (try_consume_string(ctx, TagStrings[i])) {
            tag = i;
            break;
        }
    }
    if (tag == TagCount) {
        die(ctx, "tag");
    }

    consume_char(ctx, ':');
    isize start = parser_position(ctx);
    advance_to(ctx, ';');
    isize end = parser_position(ctx);
    consume_char(ctx, ';');
    return (SmTagValue) {
        .tag = tag,
        .str.index = (i32)start,
        .str.len = (i32)(end - start)
    };
}

static b32 try_advance_to_and_parse_tag(SmParser *ctx, SmTagValue *out_tag)
{
    consume_whitespace_and_comments(ctx);
    if (*ctx->p != '#') {
        return false;
    }
    *out_tag = parse_tag(ctx);
    return true;
}

static SmString parse_notes_mode(SmParser *ctx)
{
    i32 start = parser_position(ctx);
    advance_to(ctx, ':');
    i32 end = parser_position(ctx);
    consume_char(ctx, ':');
    return (SmString) { start, end - start };
}

static SmString parse_notes_author(SmParser *ctx)
{
    i32 start = parser_position(ctx);
    advance_to(ctx, ':');
    i32 end = parser_position(ctx);
    consume_char(ctx, ':');
    return (SmString) { start, end - start };
}

SmDifficulty parse_notes_difficulty(SmParser *ctx)
{
    SmDifficulty result = DiffCount;
    for (isize i = 0; i < DiffCount; i++) {
        if (try_consume_string(ctx, DifficultyStrings[i])) {
            result = i;
            break;
        }
    }
    if (result == DiffCount) {
        // TODO: this should be edit, of which there can be more than one
        die(ctx, "difficulty");
    }
    consume_char(ctx, ':');
    return result;
}

static SmString parse_notes_difficulty_desc(SmParser *ctx)
{
    i32 start = parser_position(ctx);
    advance_to(ctx, ':');
    i32 end = parser_position(ctx);
    consume_char(ctx, ':');
    return (SmString) { start, end - start };
}

static SmString parse_notes_groove_radar(SmParser *ctx)
{
    i32 start = parser_position(ctx);
    advance_to(ctx, ':');
    i32 end = parser_position(ctx);
    consume_char(ctx, ':');
    return (SmString) { start, end - start };
}

static SmFileRow parse_notes_row(SmParser *ctx)
{
    SmFileRow result = {0};
    validate(ctx, ctx->p + 4 < ctx->end, "unexpected end of file");

    for (isize i = 0; i < 4; i++) {
        validate(ctx, SmFileNoteValid[(u8)*ctx->p], "invalid note");
        result.columns[i] = SmFileNote[(u8)*ctx->p];
        ctx->p++;
    }

    consume_whitespace_and_comments(ctx);
    return result;
}

#pragma float_control(precise, on, push)
static BPMChange parse_bpm_change(SmParser *ctx)
{
    BPMChange result = {0};
    result.row = roundf(parse_f32(ctx) * 48.0f);
    result.beat = result.row / 48.0f;
    consume_char(ctx, '=');
    f32 bpm = parse_f32(ctx);
    result.bps = bpm / 60.f;
    result.bpm = result.bps * 60.f;
    return result;
}
#pragma float_control(pop)

static b32 try_parse_next_bpm_change(SmParser *ctx, BPMChange *out_change)
{
    if (try_consume_char(ctx, ',')) {
        *out_change = parse_bpm_change(ctx);
        return true;
    }

    return false;
}

static b32 parse_next_stop(SmParser *ctx, SmStop *out_stop)
{
    SmStop result = {0};
    if (try_consume_char(ctx, ';')) {
        return false;
    }
    try_consume_char(ctx, ',');
    result.row = roundf(parse_f32(ctx) * 48.0f);
    result.beat = result.beat / 48.0f;
    consume_char(ctx, '=');
    result.duration = parse_f32(ctx);
    *out_stop = result;
    return true;
}

static const u8 RowToSnap[] = {
    4,  192, 96, 64, 48, 192, 32, 192, 24, 64, 96, 192,
    16, 192, 96, 64, 12, 192, 32, 192, 48, 64, 96, 192,
    8,  192, 96, 64, 48, 192, 32, 192, 12, 64, 96, 192,
    16, 192, 96, 64, 24, 192, 32, 192, 48, 64, 96, 192,
    4,  192, 96, 64, 48, 192, 32, 192, 24, 64, 96, 192,
    16, 192, 96, 64, 12, 192, 32, 192, 48, 64, 96, 192,
    8,  192, 96, 64, 48, 192, 32, 192, 12, 64, 96, 192,
    16, 192, 96, 64, 24, 192, 32, 192, 48, 64, 96, 192,
    4,  192, 96, 64, 48, 192, 32, 192, 24, 64, 96, 192,
    16, 192, 96, 64, 12, 192, 32, 192, 48, 64, 96, 192,
    8,  192, 96, 64, 48, 192, 32, 192, 12, 64, 96, 192,
    16, 192, 96, 64, 24, 192, 32, 192, 48, 64, 96, 192,
    4,  192, 96, 64, 48, 192, 32, 192, 24, 64, 96, 192,
    16, 192, 96, 64, 12, 192, 32, 192, 48, 64, 96, 192,
    8,  192, 96, 64, 48, 192, 32, 192, 12, 64, 96, 192,
    16, 192, 96, 64, 24, 192, 32, 192, 48, 64, 96, 192,
};
static_assert(sizeof(RowToSnap) == 192, "RowToSnap must have value for every snap");

enum
{
    Snap_Sentinel = 255
};

static u8 row_to_snap(i32 numer, i32 denom)
{
    assert(numer >= 0 && numer <= 192);
    assert(denom >= 0 && denom <= 192);
    assert(numer < denom);
    i32 n = (numer * 192) / denom;
    return RowToSnap[n];
}

static b8 row_has_tap(SmRow r)
{
    u32 mask = (Note_Tap << 24) + (Note_Tap << 16) + (Note_Tap << 8) + Note_Tap;
    return (r.cccc & mask) != 0;
}

static f32 row_time(BPMChange bpm, f32 row)
{
    return bpm.time + ((row - bpm.row) / 48.0f) / bpm.bps;
}

// Not normative: multiple rows can lie on the same time tick
static f32 time_row_naive(BPMChange bpm, f32 time)
{
    return (bpm.beat + (f32)(time - bpm.time) * bpm.bps);
}

typedef struct MinaRowTimeGarbage
{
    BPMChange *last_seen_bpm;
    b32 have_single_bpm;
    f32 last_nerv;
    i32 first_row_of_last_segment;

    f32 event_row;
    f32 time_to_next_event;
    f32 next_event_time;
    f32 last_time;
} MinaRowTimeGarbage;

static f32 mina_row_time(MinaRowTimeGarbage *g, BPMChange *bpms, f32 row)
{
    if (g->have_single_bpm) {
        return row_time(bpms[0], row);
    }

    if (g->last_seen_bpm != bpms) {
        assert(row >= g->event_row);

        BPMChange *next = g->last_seen_bpm ? g->last_seen_bpm + 1 : bpms;
        BPMChange *end = bpms + 1;
        for (; next != end; next++) {
            g->event_row = next[1].row;
            if (g->event_row == FLT_MAX) {
                g->event_row = g->last_nerv;
            }
            g->time_to_next_event = ((g->event_row - next[0].row) / 48.0f) / next[0].bps;
            g->last_time = g->next_event_time;
            g->next_event_time = g->last_time + g->time_to_next_event;
        }

        g->last_seen_bpm = bpms;
    }

    f32 perc = (row - bpms[0].row) / (g->event_row - bpms[0].row);
    return g->last_time + g->time_to_next_event * perc;
}

static i32 parse_sm(Buffer data, SmFile *out)
{
    SmFile result = { .sm = { .buf = data.buf, .len = data.len }};
    jmp_buf env;
    SmParser *ctx = &(SmParser) {
        .buf = data.buf,
        .p = data.buf,
        .end = data.buf + data.len,
        .env = &env
    };

    i32 err = setjmp(env);
    if (err) {
        printf("sm error: %s\n", ctx->error.buf);
        return err;
    }

    SmStop *stops = 0;
    b32 negBPMsexclamationmark = false;

    SmTagValue tag = {0};
    while (try_advance_to_and_parse_tag(ctx, &tag)) {
        switch (tag.tag) {
            case Tag_BPMs: {
                validate(ctx, result.file_bpms == 0, "file has two sets of bpms");

                SmParser bpm_ctx = *ctx;
                bpm_ctx.p = &ctx->buf[tag.str.index];

                // SM is forgiving on files; we aren't, and don't care about files with zero bpms.
                buf_push(result.file_bpms, parse_bpm_change(&bpm_ctx));
                validate(ctx, result.file_bpms->row == 0.0f, "first bpm is not at beat 0");
                for (BPMChange c; try_parse_next_bpm_change(&bpm_ctx, &c);) {
                    buf_push(result.file_bpms, c);
                    negBPMsexclamationmark |= (c.bps < 0);
                }
                buf_push(result.file_bpms, SentinelBPM);
            } break;
            case Tag_Delays: {
                validate(ctx, ctx->buf[tag.str.index] == ';', "delays are not supported");
                result.tags[tag.tag] = tag.str;
            } break;
            case Tag_Stops: {
                validate(ctx, stops == 0, "file has two sets of stops");
                result.tags[tag.tag] = tag.str;

                push_allocator(scratch);
                SmParser stop_ctx = *ctx;
                stop_ctx.p = &ctx->buf[tag.str.index];
                for (SmStop stop; parse_next_stop(&stop_ctx, &stop);) {
                    validate(ctx, stop.duration >= 0.0f, "stop has non-positive duration");
                    buf_push(stops, stop);
                }
                pop_allocator();
            } break;
            case Tag_Notes: {
                SmParser notes_ctx = *ctx;
                notes_ctx.p = &ctx->buf[tag.str.index];
                SmString mode = parse_notes_mode(&notes_ctx);
                SmString author = parse_notes_author(&notes_ctx);
                i32 diff = parse_notes_difficulty(&notes_ctx);
                SmString desc = parse_notes_difficulty_desc(&notes_ctx);
                SmString radar = parse_notes_groove_radar(&notes_ctx);
                i32 notes_start = parser_position(&notes_ctx);
                i32 notes_len = tag.str.len - (notes_start - tag.str.index);
                validate(ctx, notes_len > 0, "no notes");
                SmString notes = (SmString) { notes_start, notes_len };

                if (string_matches((String) { &ctx->buf[mode.index], mode.len }, "dance-single")) {
                    buf_push(result.diffs, (SmDiff) {
                        .diff = diff,
                        .mode = mode,
                        .author = author,
                        .desc = desc,
                        .radar = radar,
                        .notes = notes,
                    });
                }
            } break;
            default: {
                result.tags[tag.tag] = tag.str;
            } break;
        }
    }

    validate(ctx, ctx->p == ctx->end || *ctx->p == 0, "failed to reach end of file");
    validate(ctx, result.file_bpms != 0, "file has no bpms");

    f32 *inserted_beats = 0;

    if (buf_len(stops) == 0 && negBPMsexclamationmark == false) {
        BPMChange *bpms = result.file_bpms;
        for (i32 i = 1; i < buf_len(bpms) - 1; i++) {
            validate(ctx, bpms[i].row >= bpms[i - 1].row, "bpm row is out of order");
            bpms[i].time = row_time(bpms[i - 1], bpms[i].row);
            validate(ctx, bpms[i].time >= bpms[i - 1].time, "bpm time is out of order");
        }
        result.bpms = bpms;
    } else {
        buf_push(stops, (SmStop) { .row = FLT_MAX, .duration = 0 });
        BPMChange *bpms = result.file_bpms;

        // Deal with stops and negative bpms. In both cases we turn them into
        // equivalent bpm changes before looking at #notes at all.
        //
        // Stops: replace every stop with 48 rows (1 beat) that take as long as
        // that stop.
        //
        // In principle, we could stuff stop times into bpm changes that resolve
        // in less than one row, then recompute bpms. But this means we lose the
        // original bpms.
        //
        // NegBPMs: Compute how long it lasts and replace the negative bpm with
        // a sufficiently high bpm, and move the following BPM forward in
        // row-space.

        f32 inserted_rows = 0.0;

        BPMChange *new_bpms = 0;
        buf_push(new_bpms, bpms[0]);
        SmStop *stop = stops;
        for (i32 i = 1; i < buf_len(bpms) - 1; i++) {
            validate(ctx, bpms[i].row > bpms[i - 1].row, "bpm row is out of order");
            BPMChange next = bpms[i];

            next.row += inserted_rows;
            next.time = row_time(buf_last(new_bpms), next.row);

            if (buf_last(new_bpms).bps < 0) {
                BPMChange prev = buf_last(new_bpms);

                // row s.t. row_time(next, row) == prev.time
                f32 row = time_row_naive(next, prev.time);

                prev.bps = 10500.f * (row - prev.row);
                prev.bpm = prev.bps * 60.0f;

                buf_last(new_bpms) = prev;
                next.time = prev.time;
                next.row = row;

                // This means the bpm changed twice before the negative bpm warp was over.
                // Technically possible but probably extremely rare.
                validate(ctx, prev.row < bpms[i + 1].row, "what even is this file. don't do that");
            }

            while (stop->row + inserted_rows <= next.row) {
                f32 bps = 1.0f / stop->duration;
                f32 corrected_row = stop->row + inserted_rows;

                BPMChange bpm_stop = (BPMChange) {
                    .bps = bps,
                    .bpm = bps * 60.0f,
                    .row = corrected_row,
                    .time = row_time(buf_last(new_bpms), corrected_row),
                };

                f32 end_row = corrected_row + 48.0f;
                f32 end_time = row_time(bpm_stop, end_row);
                buf_push(new_bpms, bpm_stop);

                if (corrected_row < next.row) {
                    BPMChange end_stop = bpms[i - 1];
                    end_stop.row = end_row;
                    end_stop.time = end_time;
                    buf_push(new_bpms, end_stop);
                }

                buf_push(inserted_beats, corrected_row);

                inserted_rows += 48.0f;
                next.row += 48.0f;
                next.time = row_time(buf_last(new_bpms), next.row);
                stop++;
            }

            assert(buf_last(new_bpms).row != next.row);
            buf_push(new_bpms, next);
        }

        buf_push(new_bpms, SentinelBPM);
        result.bpms = new_bpms;
        result.has_stops = true;
    }

    buf_push(inserted_beats, FLT_MAX);

    result.n_bpms = (i32)buf_len(result.bpms);

    SmFileRow *file_rows = alloc_scratch(SmFileRow, 192);
    for (isize d = 0; d < buf_len(result.diffs); d++) {
        SmRow *rows = 0;

        BPMChange *bpm = result.bpms;
        MinaRowTimeGarbage g = { .have_single_bpm = buf_len(result.bpms) == 2 };
        MinaRowTimeGarbage last_segment_g = {0};
        f32 row = 0.0;

        u8 hold_state[4] = {0};

        ctx->p = &ctx->buf[result.diffs[d].notes.index];
        while (true) {
            i32 n_rows = 0;
            while (SmFileNoteValid[(u8)*ctx->p]) {
                file_rows[n_rows++] = parse_notes_row(ctx);
            }
            validate(ctx, n_rows > 0, "empty measures aren't supported");
            validate(ctx, n_rows <= 192, "too many rows in measure");
            f32 row_inc = 192.0f / (f32)n_rows;
            validate(ctx, (f32)(i32)(row_inc) == row_inc, "number of rows in measure does not form valid snap");

            for (i32 i = 0; i < n_rows; i++) {
                if (file_rows[i].cccc) {
                    buf_push(rows, (SmRow) {
                        .row = row,
                        .time = mina_row_time(&g, bpm, (f32)row),
                        .snap = row_to_snap(i, n_rows),
                        .cccc = file_rows[i].cccc
                    });

                    for (isize c = 0; c < 4; c++) {
                        if (file_rows[i].columns[c] & (Note_HoldStart|Note_RollStart)) {
                            validate(ctx, hold_state[c] == false, "mismatched hold head");
                            hold_state[c] = true;
                        } else if (file_rows[i].columns[c] == Note_HoldEnd) {
                            validate(ctx, hold_state[c] == true, "mismatched hold end");
                            hold_state[c] = false;
                        }
                    }
                }

                row += row_inc;

                while (row > *inserted_beats) {
                    row += 48.0f;
                    inserted_beats++;
                }

                while (row >= bpm[1].row) {
                    bpm++;

                    if (bpm[1].row == FLT_MAX) {
                        last_segment_g = g;
                        last_segment_g.first_row_of_last_segment = (i32)buf_len(rows);
                    }
                }
            }

            if (try_consume_char(ctx, ';')) {
                break;
            }

            consume_char(ctx, ',');
        }

        assert(bpm < buf_end(result.bpms));

        i32 rowi = (i32)row % 192;
        if (rowi) {
            row += 192.0f - (f32)rowi;
        }
        buf_push(rows, (SmRow) { .time = row_time(*bpm, row), .snap = Snap_Sentinel });

        // mina_row_time needs to know the last non empty row for the last segment.
        // We're single pass, so this fix up is needed.
        if (g.have_single_bpm == false) {
            last_segment_g.last_nerv = buf_end(rows)[-2].row;
            last_segment_g.last_seen_bpm = bpm - 1;
            for (isize i = last_segment_g.first_row_of_last_segment; i < buf_len(rows) - 1; i++) {
                rows[i].time = mina_row_time(&last_segment_g, bpm, rows[i].row);
            }
        }

        result.diffs[d].rows = rows;
        result.diffs[d].n_rows = (i32)buf_len(rows);

        for (i32 i = 1; i < result.diffs[d].n_rows; i++) {
            assert(rows[i - 1].time <= rows[i].time);
        }
    }

    if (out) {
        *out = result;
    }
    return 0;
}

typedef union MinaSerializedRow
{
    u8 taps[4];
    u32 row;
} MinaSerializedRow;

static MinaSerializedRow row_mina_serialize(SmRow r)
{
    static u8 tap_note_type[256] = {
	    [Note_Off] = 0,
	    [Note_On] = 1,
	    [Note_HoldStart] = 2,
	    [Note_RollStart] = 2,
	    [Note_Mine] = 4,
	    [Note_Lift] = 5,
	    [Note_Fake] = 7,
    };
    MinaSerializedRow result = {0};
    result.taps[0] = tap_note_type[r.columns[0]];
    result.taps[1] = tap_note_type[r.columns[1]];
    result.taps[2] = tap_note_type[r.columns[2]];
    result.taps[3] = tap_note_type[r.columns[3]];
    return result;
}

static u32 mina_row_bits(SmRow r)
{
    return ((u32)((r.columns[0] & Note_Tap) != 0) << 0)  // left
         + ((u32)((r.columns[1] & Note_Tap) != 0) << 1)  // left
         + ((u32)((r.columns[2] & Note_Tap) != 0) << 2)  // right
         + ((u32)((r.columns[3] & Note_Tap) != 0) << 3); // right
}

#pragma float_control(precise, on, push)
NoteInfo *sm_to_ett_note_info(SmFile *sm, i32 diff)
{
    NoteInfo *result = 0;
    SmRow *r = sm->diffs[diff].rows;

    SmString offset_str = sm->tags[Tag_Offset];
    f32 offset = strtof(&sm->sm.buf[offset_str.index], 0);

    if (row_mina_serialize(r[0]).row == 0) {
        for (i32 i = 1; i < sm->diffs[diff].n_rows; i++) {
            MinaSerializedRow row = row_mina_serialize(r[i]);
            if (row.row) {
                offset += r[i].time;
                break;
            }
        }
    }

    for (i32 i = 0; i < sm->diffs[diff].n_rows; i++) {
        u32 notes = mina_row_bits(r[i]);
        if (notes) {
            buf_push(result, (NoteInfo) {
                .notes = notes,
                .rowTime = (r[i].time - offset) - (r[0].time - offset)
            });
        }
    }

    return result;
}
#pragma float_control(pop)

typedef union SHA1Hash
{
    u8 hash[20];
    u32 words[5];
} SHA1Hash;

static void sha1_chunk(u32 *data, u32 h[5])
{
    #define leftrotate(x, n) (((x) << (n)) + ((x) >> (32 - (n))))

    u32 w[80] = {0};

    for (isize i = 0; i < 16; i++) {
        w[i] = (data[i] >> 24)
            + ((data[i] >> 8) & 0x0000ff00)
            + ((data[i] << 8) & 0x00ff0000)
            +  (data[i] << 24);
    }

    for (isize i = 16; i < 80; i++) {
        u32 ww = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
        w[i] = leftrotate(ww, 1);
    }

    u32 a = h[0];
    u32 b = h[1];
    u32 c = h[2];
    u32 d = h[3];
    u32 e = h[4];

    for (isize i = 0; i < 20; i++) {
        u32 f = (b & c) | ((~b) & d);
        u32 k = 0x5A827999;
        u32 temp = leftrotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = leftrotate(b, 30);
        b = a;
        a = temp;
    }
    for (isize i = 20; i < 40; i++) {
        u32 f = b ^ c ^ d;
        u32 k = 0x6ED9EBA1;
        u32 temp = leftrotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = leftrotate(b, 30);
        b = a;
        a = temp;
    }
    for (isize i = 40; i < 60; i++) {
        u32 f = (b & c) | (b & d) | (c & d);
        u32 k = 0x8F1BBCDC;
        u32 temp = leftrotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = leftrotate(b, 30);
        b = a;
        a = temp;
    }
    for (isize i = 60; i < 80; i++) {
        u32 f = b ^ c ^ d;
        u32 k = 0xCA62C1D6;
        u32 temp = leftrotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = leftrotate(b, 30);
        b = a;
        a = temp;
    }

    h[0] = h[0] + a;
    h[1] = h[1] + b;
    h[2] = h[2] + c;
    h[3] = h[3] + d;
    h[4] = h[4] + e;

    #undef leftrotate
}

static SHA1Hash sha1(u8 *data, isize len)
{
    // https://en.wikipedia.org/w/index.php?title=SHA-1&oldid=966694894

    u64 ml = len * 8;

    u32 h[5] = {
        0x67452301,
        0xEFCDAB89,
        0x98BADCFE,
        0x10325476,
        0xC3D2E1F0,
    };

    u8 *chunk = data;
    for (isize i = 0; i < len / 64; i++) {
        sha1_chunk((u32 *)chunk, h);
        chunk += 64;
    }

    u8 buf[64] = {0};
    isize remaining = (data + len) - chunk;
    memcpy(buf, chunk, remaining);
    buf[remaining] = 0x80;
    if (remaining >= 56) {
        sha1_chunk((u32 *)buf, h);
        memset(buf, 0, sizeof(buf));
    }

    u8 *length = buf + sizeof(buf) - sizeof(u64);
    length[0] = (ml >> 56) & 0xff;
    length[1] = (ml >> 48) & 0xff;
    length[2] = (ml >> 40) & 0xff;
    length[3] = (ml >> 32) & 0xff;
    length[4] = (ml >> 24) & 0xff;
    length[5] = (ml >> 16) & 0xff;
    length[6] = (ml >>  8) & 0xff;
    length[7] = (ml      ) & 0xff;

    sha1_chunk((u32 *)buf, h);

    SHA1Hash result = {0};
    for (isize i = 0; i < array_length(result.words); i++) {
        result.words[i] = (h[i] >> 24)
                       + ((h[i] >> 8) & 0x0000ff00)
                       + ((h[i] << 8) & 0x00ff0000)
                       +  (h[i] << 24);
    }

    return result;
}

String generate_chart_key(SmFile *sm, isize diff)
{
	BPMChange *bpm = sm->bpms;
    SmRow *r = sm->diffs[diff].rows;
    char *prep = 0;
	for (isize i = 0; i < sm->diffs[diff].n_rows; i++) {
        MinaSerializedRow row = row_mina_serialize(r[i]);
        if (row.row) {
            while (r[i].row >= bpm[1].row) {
                bpm++;
            }
            buf_printf(prep, "%d%d%d%d%d",
                row.taps[0],
                row.taps[1],
                row.taps[2],
                row.taps[3],
                (int)(bpm->bpm + 0.374643f)
            );
        }
	}

    SHA1Hash hash = sha1(prep, buf_len(prep));

    String result = {
        .buf = buf_make((char) {'X'}),
    };
    for (isize i = 0; i < array_length(hash.hash); i++) {
        buf_printf(result.buf, "%02x", hash.hash[i]);
    }
    result.len = buf_len(result.buf);
    buf_push(result.buf, 0);

    return result;
}
