enum {
    NumGraphSamples = 19
};

struct FnGraph
{
    b32 active;
    b32 is_param;
    i32 param;
    i32 len;

    f32 xs[NumGraphSamples];
    f32 ys[NumSkillsets][NumGraphSamples];
    f32 relative_ys[NumSkillsets][NumGraphSamples];

    f32 min;
    f32 max;
    f32 relative_min;

    u32 generation;
    u32 pending_generation;
};

typedef enum {
    Work_Invalid,
    Work_Wife = 1,
    Work_Parameter = 2,
    Work_ParameterLowerBound = 4 | Work_Parameter,
    Work_ParameterUpperBound = 8 | Work_Parameter,
    Work_Effects = 16,
} WorkType;

typedef struct CalcThread
{
    CalcInfo *info;
    ParamSet *ps;
    u32 *generation;

    struct {
        usize skipped;
        usize discarded;
        usize done;
    } debug_counters;
} CalcThread;

typedef struct CalcWork
{
    SimFileInfo *sfi;
    i32 type;
    i32 x_index;
    union {
        struct {
            f32 goal;
        } wife;
        struct {
            f32 bound;
            i32 param;
            f32 value;
            i32 param_of_fng;
        } parameter;
        struct {
            i32 start;
            i32 end;
        } effects;
    };
    u32 generation;
} CalcWork;

static f32 rating_floor(f32 v)
{
    return (f32)((i32)(v * 100.0f)) / 100.f;
}

void calculate_effect_for_param(CalcInfo *info, SeeCalc *calc, NoteData *note_data, i32 param, SkillsetRatings *default_ratings, EffectMasks *out)
{
    i32 p = param;


    SkillsetRatings ratings = {0};

    // stronk. they react to small changes at 93%
    {
        f32 value = 0.0f;
        if (info->params[p].default_value == 0) {
            value = 1.0f;
        } else if (info->params[p].default_value > info->params[p].min) {
            value = info->params[p].default_value * 0.95f;
        } else {
            value = info->params[p].default_value * 1.05f;
        }

        ratings = calc_go_with_param(calc, &info->defaults, note_data, 1.0f, 0.93f, param, value);

        for (int r = 0; r < NumSkillsets; r++) {
            b32 changed = rating_floor(ratings.E[r]) != default_ratings->E[r];
            out->strong[p] |= (changed << r);
        }
    }

    // weak. they react to big changes at 96.5%, and not small changes at 93%
    {
        f32 distance_from_low = info->params[p].default_value - info->params[p].min;
        f32 distance_from_high = info->params[p].max - info->params[p].max;
        assert(info->params[p].max >= info->params[p].min);
        f32 value = 0.f;
        if (info->params[p].default_value == 0) {
            value = 100.0f;
        } else if (distance_from_low > distance_from_high) {
            value = info->params[p].min;
        } else {
            value = info->params[p].max;
        }

        ratings = calc_go_with_param(calc, &info->defaults, note_data, 1.0f, 0.93f, param, value);

        out->weak[p] = out->strong[p];
        for (i32 r = 0; r < NumSkillsets; r++) {
            b32 changed = rating_floor(ratings.E[r]) != default_ratings->E[r];
            out->weak[p] |= (changed << r);
        }
    }
}

typedef struct DoneWork
{
    CalcWork work;
    SkillsetRatings ssr;
} DoneWork;

#if defined(_MSC_VER) && !defined(alignas)
#define alignas(n) __declspec(align(n))
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

typedef struct WorkQueue
{
    alignas(64) usize read;
    alignas(64) usize write;
    CalcWork entries[4096];
    int lock_id;
} WorkQueue;

typedef struct DoneQueue
{
    alignas(64) usize read;
    alignas(64) usize write;
    DoneWork entries[4096];
    int lock_id;
} DoneQueue;

// High prio queue gets work for currently open windows
// Low prio queue gets all work, including those in the high prio queue
// Low prio is only accessed if high prio is empty
// A generation counter is used to skip out of date work
static WorkQueue high_prio_work_queue = {0};
static WorkQueue low_prio_work_queue = {0};
static DoneQueue done_queue = {0};

struct {
    usize requested;
    usize skipped;
    usize done;
} debug_counters = {0};

static const usize WorkQueueMask = array_length(low_prio_work_queue.entries) - 1;
static_assert((array_length(low_prio_work_queue.entries) & (array_length(low_prio_work_queue.entries) - 1)) == 0, "");
static_assert(array_length(low_prio_work_queue.entries) == array_length(done_queue.entries), "");


void calculate_file_graphs(CalcWork *work[], SimFileInfo *sfi, u32 generation)
{
    // Skillsets over wife
    {
        FnGraph *fng = &state.graphs[sfi->graphs[0]];
        if (fng->pending_generation < generation) {
            fng->pending_generation = generation;
            for (isize i = 0; i < NumGraphSamples; i++) {
                buf_push(*work, (CalcWork) {
                    .sfi = sfi,
                    .type = Work_Wife,
                    .wife.goal = fng->xs[i] / 100.0f,
                    .x_index = (i32)i,
                    .generation = state.generation,
                });
            }
        }
    }

    // Skillsets over parameter
    for (isize i = buf_len(sfi->graphs) - 1; i >= 1; i--) {
        FnGraph *fng = &state.graphs[sfi->graphs[i]];
        if (fng->pending_generation < generation) {
            fng->pending_generation = generation;
            for (isize sample = 0; sample < NumGraphSamples; sample++) {
                buf_push(*work, (CalcWork) {
                    .sfi = sfi,
                    .type = Work_Parameter,
                    .x_index = (i32)sample,
                    .parameter.param = fng->param,
                    .parameter.value = lerp(state.ps.min[fng->param], state.ps.max[fng->param], (f32)sample / (NumGraphSamples - 1)),
                    .parameter.param_of_fng = fng->param,
                    .generation = state.generation,
                });
            }
        }
    }
}

void calculate_graphs_in_background(CalcWork *work[], WorkType type, i32 param, f32 value)
{
    if (type == Work_Wife) {
        // 93 and 96.5, for the files list
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Wife,
                .x_index = 11,
                .wife.goal = 0.93f,
                .generation = state.generation,
            });
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Wife,
                .x_index =  NumGraphSamples - 1,
                .wife.goal = 1.0f,
                .generation = state.generation,
            });
        }

        // Skillsets over wife
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (sfi->open == false) {
                FnGraph *fng = &state.graphs[sfi->graphs[0]];
                for (isize i = 0; i < NumGraphSamples - 1; i++) {
                    if (i != 11) {
                        buf_push(*work, (CalcWork) {
                            .sfi = sfi,
                            .type = Work_Wife,
                            .wife.goal = fng->xs[i] / 100.0f,
                            .x_index = (i32)i,
                            .generation = state.generation,
                        });
                    }
                }
            }
        }

        // Skillsets over parameter (that aren't the one that just changed)
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            for (isize i = 1; i < buf_len(sfi->graphs); i++) {
                if (sfi->open == false) {
                    FnGraph *fng = &state.graphs[sfi->graphs[i]];
                    if (fng->param != param) {
                        for (isize sample = 0; sample < NumGraphSamples; sample++) {
                            buf_push(*work, (CalcWork) {
                                .sfi = sfi,
                                .type = Work_Parameter,
                                .x_index = (i32)sample,
                                .parameter.param = fng->param,
                                .parameter.value = lerp(state.ps.min[fng->param], state.ps.max[fng->param], (f32)sample / (NumGraphSamples - 1)),
                                .parameter.param_of_fng = fng->param,
                                .generation = state.generation,
                            });
                        }
                    }
                }
            }
        }
    }

    if ((type & Work_Parameter) == Work_Parameter) {
        // Skillsets over parameter (that is the one that just changed)
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            for (isize i = 1; i < buf_len(sfi->graphs); i++) {
                if (sfi->open == false) {
                    FnGraph *fng = &state.graphs[sfi->graphs[i]];
                    if (fng->param == param) {
                        for (isize sample = 0; sample < NumGraphSamples; sample++) {
                            buf_push(*work, (CalcWork) {
                                .sfi = sfi,
                                .type = type,
                                .x_index = (i32)sample,
                                .parameter.bound = value,
                                .parameter.param = param,
                                .parameter.value = lerp(state.ps.min[param], state.ps.max[param], (f32)sample / (NumGraphSamples - 1)),
                                .parameter.param_of_fng = param,
                                .generation = state.generation,
                            });
                        }
                    }
                }
            }
        }
    }
}

void calculate_effects(CalcWork *work[], CalcInfo *info, SimFileInfo *sfi)
{
    i32 stride = 8;
    i32 i = 1;
    buf_push(*work, (CalcWork) {
        .sfi = sfi,
        .type = Work_Effects,
        .effects.start = 0,
        .effects.end = 1,
        .generation = state.generation,
    });
    for (; i < info->num_params; i += stride) {
        buf_push(*work, (CalcWork) {
            .sfi = sfi,
            .type = Work_Effects,
            .effects.start = i,
            .effects.end =  (i32)mins(i + stride, info->num_params),
            .generation = state.generation,
        });
    }
}

b32 get_done_work(DoneWork *out)
{
    usize read = done_queue.read;
    usize write = done_queue.write;

    if (read == write) {
        return false;
    }

    *out = done_queue.entries[read & WorkQueueMask];
    wag_tail();
    done_queue.read++;
    return true;
}

b32 get_work(CalcThread *ct, WorkQueue *q, CalcWork *out)
{
    b32 ok = false;

    if (q->read == q->write) {
        return false;
    }

    lock(q->lock_id);

    usize read = q->read;
    usize write = q->write;
    u32 generation = * ct->generation;

    CalcWork *cw = 0;
    usize r = read;
    for (; r != write; r++) {
        cw = q->entries + (r & WorkQueueMask);
        if (cw->generation == generation) {
            break;
        }
    }

    if (r != write) {
        ok = true;
        *out = *cw;
        q->read = r + 1;
    } else {
        q->read = r;
    }

    unlock(q->lock_id);

    ct->debug_counters.skipped += r - read;
    return ok;
}

i32 calc_thread(void *userdata)
{
    CalcThread *ct = userdata;
    SeeCalc calc = calc_init(ct->info);
    ParamSet *ps = ct->ps;

    while (true) {
        CalcWork work = {0};
        while (get_work(ct, &high_prio_work_queue, &work) || get_work(ct, &low_prio_work_queue, &work)) {
            SkillsetRatings ssr = {0};
            switch (work.type) {
                case Work_ParameterLowerBound:
                case Work_ParameterUpperBound: {
                    // Generations invalidate every graph, but these only
                    // invalidate one. So special case it here.
                    f32 *bounds = (work.type == Work_ParameterLowerBound) ? ps->min : ps->max;
                    if (bounds[work.parameter.param] == work.parameter.bound) {
                        ssr = calc_go_with_param(&calc, ps, work.sfi->notes, 1.0f, 0.93f, work.parameter.param, work.parameter.value);
                    } else {
                        ct->debug_counters.skipped++;
                        continue;
                    }
                } break;
                case Work_Parameter: {
                    ssr = calc_go_with_param(&calc, ps, work.sfi->notes, 1.0f, 0.93f, work.parameter.param, work.parameter.value);
                } break;
                case Work_Wife: {
                    ssr = calc_go(&calc, ps, work.sfi->notes, 1.0f, work.wife.goal);
                } break;
                case Work_Effects: {
                    ssr = calc_go(&calc, &ct->info->defaults, work.sfi->notes, 1.0f, 0.93f);
                    for (i32 r = 0; r < NumSkillsets; r++) {
                        ssr.E[r] = rating_floor(ssr.E[r]);
                    }
                    for (i32 i = work.effects.start; i < work.effects.end; i++) {
                        calculate_effect_for_param(ct->info, &calc, work.sfi->notes, i, &ssr, &work.sfi->effects);
                    }
                } break;
                default: assert_unreachable();
            }

            lock(done_queue.lock_id);
            done_queue.entries[done_queue.write++ & WorkQueueMask] = (DoneWork) {
                .work = work,
                .ssr = ssr
            };
            unlock(done_queue.lock_id);

            ct->debug_counters.done++;
        }

        thread_wait();
    }

    return 0;
}

void submit_work(WorkQueue *q, CalcWork work[], u32 generation)
{
    // Loading queue.read (but not queue.write) is a race condition. The
    // consequence is always that max_to_submit is smaller than it could be,
    // which is fine
    isize max_to_submit = (array_length(q->entries) - 1) - (q->write - q->read);
    assert(0 <= max_to_submit && max_to_submit < 4096);

    isize N = mins(buf_len(work), max_to_submit);
    isize n = 0;
    for (isize i = 0; i < N; i++) {
        if (work[i].generation == generation) {
            q->entries[(q->write + i) & WorkQueueMask] = work[i];
            n++;
        }
    }

    wag_tail();
    q->write += n;
    thread_notify();

    debug_counters.requested += n;

    memmove(work, work + N, sizeof(CalcWork) * (buf_len(work) - N));
    buf_set_len(work, -N);
}

void finish_work()
{
    DoneWork done = {0};
    while (get_done_work(&done)) {
        SimFileInfo *sfi = done.work.sfi;
        FnGraph *fng = 0;
        if (done.work.type == Work_Wife) {
            fng = &state.graphs[done.work.sfi->graphs[0]];
        } else if ((done.work.type & Work_Parameter) == Work_Parameter) {
            for (isize i = 1; i < buf_len(sfi->graphs); i++) {
                if (state.graphs[sfi->graphs[i]].param == done.work.parameter.param_of_fng) {
                    fng = &state.graphs[sfi->graphs[i]];
                    break;
                }
            }

            if (fng == 0) {
                // fng has been removed since calc calculation completed
                continue;
            }
        } else {
            assert(done.work.type == Work_Effects);
            for (isize ss = 1; ss < NumSkillsets; ss++) {
                sfi->selected_skillsets[ss] = sfi->display_skillsets[ss];
            }

            continue;
        }

        if (fng->generation > done.work.generation) {
            continue;
        }
        fng->generation = done.work.generation;

        f32 min_y = 100.f;
        f32 rel_min_y = 0.0f;
        for (isize ss = 0; ss < NumSkillsets; ss++) {
            fng->xs[done.work.x_index] = (done.work.type == Work_Wife) ? done.work.wife.goal * 100.f : done.work.parameter.value;
            fng->ys[ss][done.work.x_index] = done.ssr.E[ss];
            fng->relative_ys[ss][done.work.x_index] = done.ssr.E[ss] / done.ssr.overall;

            if (fng->ys[ss][0] < min_y) {
                min_y = fng->ys[ss][0];
                rel_min_y = safe_div(min_y, fng->ys[0][0]);
            }
        }
        fng->min = min_y;
        fng->relative_min = rel_min_y;
        fng->max = fng->ys[0][NumGraphSamples - 1] ? fng->ys[0][NumGraphSamples - 1] : 40.0f;

        for (isize ss = 1; ss < NumSkillsets; ss++) {
            FnGraph *w = &state.graphs[sfi->graphs[0]];
            sfi->display_skillsets[ss] = (0.9f <= (w->ys[ss][11] / w->ys[0][NumGraphSamples - 1]));
        }
    }
}
