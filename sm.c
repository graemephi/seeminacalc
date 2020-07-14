static void die(SmParser *ctx)
{
    i32 current_line = 1;
    if (ctx->p) {
        for (char *p = ctx->p; p != ctx->buf; p--) {
            if (*p == '\n') {
                current_line++;
            }
        }
    }
    printf("parse error at line %d\n", current_line);
    exit(1);
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
    char *p = ctx->p;
    while (*p && (u8)*p <= (u8)' ' && p < ctx->end) {
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
        die(ctx);
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
        die(ctx);
    }
}

static f32 parse_f32(SmParser *ctx)
{
    char *p = ctx->p;
    f32 result = strtof(p, &p);
    if (p == ctx->p) {
        die(ctx);
    }
    ctx->p = p;
    consume_whitespace_and_comments(ctx);
    return result;
}

static const u8 SmFileNote[256] = {
    ['0'] = NoteOff,
    ['1'] = NoteOn,
    ['2'] = NoteHoldStart,
    ['3'] = NoteHoldEnd,
    ['4'] = NoteRollEnd,
    ['M'] = NoteMine,
    ['K'] = NoteOff,
    ['L'] = NoteLift,
    ['F'] = NoteFake,
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

static const u8 SmNoteChar[256] = {
    [NoteOff] = '0',
    [NoteOn] = '1',
    [NoteHoldStart] = '2',
    [NoteHoldEnd] = '3',
    [NoteRollEnd] = '4',
    [NoteMine] = 'M',
    [NoteLift] = 'L',
    [NoteFake] = 'F',
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
        die(ctx);
    }

    consume_char(ctx, ':');
    isize start = parser_position(ctx);
    advance_to(ctx, ';');
    isize end = parser_position(ctx);
    consume_char(ctx, ';');
    return (SmTagValue) {
        tag,
        (i32)(start),
        (i32)(end - start)
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
        die(ctx);
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
    if (ctx->p + 4 > ctx->end) {
        die(ctx);
    }

    for (isize i = 0; i < 4; i++) {
        if (SmFileNoteValid[*ctx->p] == false) {
            die(ctx);
        }
        result.columns[i] = SmFileNote[*ctx->p];
        ctx->p++;
    }

    consume_whitespace_and_comments(ctx);
    return result;
}

static BPMChange parse_bpm_change(SmParser *ctx)
{
    BPMChange result = {0};
    result.beat = roundf(parse_f32(ctx));
    result.row = result.beat * 48.0f;
    consume_char(ctx, '=');
    f32 bpm = parse_f32(ctx);
    if (bpm == 0.0f) {
        die(ctx);
    }
    result.bps = bpm / 60.f;
    return result;
}

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
    result.beat = roundf(parse_f32(ctx));
    result.row = result.beat * 48.0f;
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
    u32 mask = (NoteTap << 24) + (NoteTap << 16) + (NoteTap << 8) + NoteTap;
    return (r.cccc & mask) != 0;
}

// Normative: every row has one and only one time. It's whatever this function says it is
static f32 row_time(BPMChange bpm, f32 row)
{
    return bpm.time + ((row - bpm.row) / 48.0f) / bpm.bps;
}

// Not normative: multiple rows can lie on the same time tick
static f32 time_row_naive(BPMChange bpm, f32 time)
{
    return (bpm.beat + (f32)(time - bpm.time) * bpm.bps);
}

static SmFile parse_sm(Buffer data)
{
    // TODO: all asserts in this function should be validation errors

    SmFile result = { .sm = { .buf = data.buf, .len = data.len }};
    SmParser *ctx = &(SmParser) { data.buf, data.buf, data.buf + data.len };

    SmStop *stops = 0;
    b32 negBPMsexclamationmark = false;

    SmTagValue tag = {0};
    while (try_advance_to_and_parse_tag(ctx, &tag)) {
        switch (tag.tag) {
            case Tag_BPMs: {
                assert(result.file_bpms == 0);
                buf_use(result.file_bpms, &permanent_memory);

                SmParser bpm_ctx = *ctx;
                bpm_ctx.p = &ctx->buf[tag.str.index];

                // SM is forgiving on files; we aren't, and don't care about files with zero bpms.
                buf_push(result.file_bpms, parse_bpm_change(&bpm_ctx));
                assert(result.file_bpms->row == 0.0f);
                for (BPMChange c; try_parse_next_bpm_change(&bpm_ctx, &c);) {
                    buf_push(result.file_bpms, c);
                    negBPMsexclamationmark |= (c.bps < 0);
                }
                buf_push(result.file_bpms, SentinelBPM);
            } break;
            case Tag_Delays: {
                assert(ctx->buf[tag.str.index] == ';');
                result.tag_values[tag.tag] = tag.str;
            } break;
            case Tag_Stops: {
                assert(stops == 0);
                result.tag_values[tag.tag] = tag.str;

                SmParser stop_ctx = *ctx;
                stop_ctx.p = &ctx->buf[tag.str.index];
                for (SmStop stop; parse_next_stop(&stop_ctx, &stop);) {
                    assert(stop.duration >= 0.0f);
                    buf_push(stops, stop);
                }
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
                assert(notes_len > 0);
                SmString notes = (SmString) { notes_start, notes_len };

                if (string_matches((String) { &ctx->buf[mode.index], mode.len }, "dance-single")) {
                    result.diffs[diff].valid = true;
                    result.diffs[diff].mode = mode;
                    result.diffs[diff].author = author;
                    result.diffs[diff].desc = desc;
                    result.diffs[diff].radar = radar;
                    result.diffs[diff].notes = notes;
                }
            } break;
            default: {
                assert(tag.tag < TagCount);
                result.tag_values[tag.tag] = tag.str;
            } break;
        }
    }

    if (*ctx->p != 0) {
        die(ctx);
    }

    assert(result.file_bpms);

    f32 *inserted_beats = 0;

    if (buf_len(stops) == 0 && negBPMsexclamationmark == false) {
        BPMChange *bpms = result.file_bpms;
        for (i32 i = 1; i < buf_len(bpms) - 1; i++) {
            assert(bpms[i].row >= bpms[i - 1].row);
            bpms[i].time = row_time(bpms[i - 1], bpms[i].row);
            assert(bpms[i].time >= bpms[i - 1].time);
        }
        result.bpms = bpms;
    } else {
#if 0
        buf_push(stops, (SmStop) { .row = DBL_MAX, .ticks = 0 });
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

        f64 inserted_rows = 0.0;

        BPMChange *new_bpms = 0;
        buf_use(new_bpms, &permanent_memory);
        buf_push(new_bpms, bpms[0]);
        SmStop *stop = stops;
        for (i32 i = 1; i < buf_len(bpms) - 1; i++) {
            assert(bpms[i].row > bpms[i - 1].row);
            BPMChange next = bpms[i];

            next.row += inserted_rows;
            next.time = row_time(buf_last(new_bpms), next.row);

            if (buf_last(new_bpms).bps < 0) {
                BPMChange prev = buf_last(new_bpms);

                // row s.t. row_time(next, row) == prev.time
                f64 row = time_row_naive(next, prev.time);

                prev.tpr = 0.25 / (row - prev.row);
                buf_last(new_bpms) = prev;
                next.time = prev.time;
                next.row = row;

                // This means the bpm changed twice before the negative bpm warp was over.
                // Technically possible but probably extremely rare.
                assert(prev.row < bpms[i + 1].row);
            }

            while (stop->row + inserted_rows <= next.row) {
                f64 ticks = stop->ticks / 48.0;
                f64 corrected_row = stop->row + inserted_rows;

                BPMChange bpm_stop = (BPMChange) {
                    .tpr = ticks,
                    .row = corrected_row,
                    .time = row_time(buf_last(new_bpms), corrected_row),
                };

                f64 end_row = corrected_row + 48.0;
                f64 end_time = row_time(bpm_stop, end_row);
                buf_push(new_bpms, bpm_stop);

                if (corrected_row < next.row) {
                    BPMChange end_stop = bpms[i - 1];
                    end_stop.row = end_row;
                    end_stop.time = end_time;
                    buf_push(new_bpms, end_stop);
                }

                buf_push(inserted_beats, corrected_row);

                inserted_rows += 48.0;
                next.row += 48.0;
                next.time = row_time(buf_last(new_bpms), next.row);
                stop++;
            }

            assert(buf_last(new_bpms).row != next.row);
            buf_push(new_bpms, next);
        }

        buf_push(new_bpms, SentinelBPM);
        result.bpms = new_bpms;
    #endif
        die(ctx);
    }

    buf_push(inserted_beats, FLT_MAX);

    result.n_bpms = (i32)buf_len(result.bpms);

    SmFileRow *file_rows = alloc_scratch(SmFileRow, 192);
    for (isize d = 0; d < DiffCount; d++) {
        if (result.diffs[d].valid) {
            SmRow *rows = 0;
            buf_use(rows, &permanent_memory);

            BPMChange *bpm = result.bpms;
            f32 row = 0.0f;

            ctx->p = &ctx->buf[result.diffs[d].notes.index];
            while (true) {
                i32 n_rows = 0;
                while (SmFileNoteValid[*ctx->p]) {
                    file_rows[n_rows++] = parse_notes_row(ctx);
                }
                assert(n_rows > 0);
                assert(n_rows <= 192);

                f32 row_inc = 192.0f / (f32)n_rows;
                assert(rint(row_inc) == row_inc);

                for (i32 i = 0; i < n_rows; i++) {
                    if (file_rows[i].cccc) {
                        buf_push(rows, (SmRow) {
                            .time = row_time(*bpm, row),
                            .snap = row_to_snap(i, n_rows),
                            .cccc = file_rows[i].cccc
                        });
                    }

                    row += row_inc;

                    while (row > *inserted_beats) {
                        row += 48.0f;
                        inserted_beats++;
                    }

                    while (row >= bpm[1].row) {
                        bpm++;
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

            result.diffs[d].rows = rows;
            result.diffs[d].n_rows = (i32)buf_len(rows);

            // Not user error here
            for (i32 i = 1; i < result.diffs[d].n_rows; i++) {
                assert(rows[i - 1].time <= rows[i].time);
            }

            consume_whitespace_and_comments(ctx);
        }
    }

    return result;
}
