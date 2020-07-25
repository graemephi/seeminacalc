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
};

enum {
    Graph_Invalid,
    Graph_Wife = 1,
    Graph_Parameter = 2,
    Graph_ParameterLowerBound = 4 | Graph_Parameter,
    Graph_ParameterUpperBound = 8 | Graph_Parameter
};

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
        f32 goal;
        f32 bound;
    };
    i32 param;
    f32 value;
    i32 param_of_fng;
    u32 generation;
} CalcWork;

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
    u32 *generation_ref = ct->generation;

    b32 ok = false;
    while (!ok) {
        usize read = q->read;
        usize write = q->write;
        u32 generation = *generation_ref;

        if (read == write) {
            break;
        }

        CalcWork *cw = 0;
        usize r = read;
        for (; r != write; r++) {
            cw = q->entries + (r & WorkQueueMask);
            if (cw->generation == generation) {
                break;
            }
        }

        if (r != write) {
            lock(q->lock_id);
            if (read == q->read) {
                ok = true;

                *out = *cw;
                q->read = r + 1;

                ct->debug_counters.skipped += r - read;
            }
            unlock(q->lock_id);
        } else if ((write - read) == (array_length(q->entries) - 1)) {
            // the entire queue is filled with out of date work we tried to skip past
            lock(q->lock_id);
            q->read = q->write;
            ct->debug_counters.skipped += (array_length(q->entries) - 1);
            unlock(q->lock_id);
        }
    }

    return ok;
}

i32 calc_thread(void *userdata)
{
    CalcThread *ct = userdata;
    SeeCalc calc = calc_init(ct->info);
    ParamSet *ps = ct->ps;

    while (true) {
        CalcWork work = {0};
        while (get_work(ct, &low_prio_work_queue, &work)) {
            SkillsetRatings ssr = {0};
            switch (work.type) {
                case Graph_ParameterLowerBound:
                case Graph_ParameterUpperBound: {
                    // Generations invalidate every graph, but these only
                    // invalidate one. So special case it here.
                    f32 *bounds = (work.type == Graph_ParameterLowerBound) ? ps->min : ps->max;
                    if (bounds[work.param] == work.bound) {
                        ssr = calc_go_with_param(&calc, ps, work.sfi->notes, 1.0f, 0.93f, work.param, work.value);
                    } else {
                        ct->debug_counters.skipped++;
                        continue;
                    }
                } break;
                case Graph_Parameter: {
                    ssr = calc_go_with_param(&calc, ps, work.sfi->notes, 1.0f, 0.93f, work.param, work.value);
                } break;
                case Graph_Wife: {
                    ssr = calc_go(&calc, ps, work.sfi->notes, 1.0f, work.goal);
                } break;
                default: assert_unreachable();
            }
            wag_tail();
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

void recalculate_graphs_in_background(CalcWork *work[], i32 param, i32 type, f32 value)
{
    if (type == Graph_Wife) {
        // 93 and 96.5, for the files list
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Graph_Wife,
                .x_index = 11,
                .goal = 0.93f,
                .param = param,
                .value = value,
                .param_of_fng = -1,
                .generation = state.generation,
            });
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Graph_Wife,
                .x_index =  NumGraphSamples - 1,
                .goal = 1.0f,
                .param = param,
                .value = value,
                .param_of_fng = -1,
                .generation = state.generation,
            });
        }

        // Skillsets over wife
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            FnGraph *fng = &state.graphs[sfi->graphs[0]];
            for (isize i = 0; i < NumGraphSamples - 1; i++) {
                if (i != 11) {
                    buf_push(*work, (CalcWork) {
                        .sfi = sfi,
                        .type = Graph_Wife,
                        .goal = fng->xs[i] / 100.0f,
                        .x_index = (i32)i,
                        .param = param,
                        .value = value,
                        .param_of_fng = -1,
                        .generation = state.generation,
                    });
                }
            }
        }

        // Skillsets over parameter (that aren't the one that just changed)
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            for (isize i = 1; i < buf_len(sfi->graphs); i++) {
                FnGraph *fng = &state.graphs[sfi->graphs[i]];
                if (fng->param != param) {
                    for (isize sample = 0; sample < NumGraphSamples; sample++) {
                        buf_push(*work, (CalcWork) {
                            .sfi = sfi,
                            .type = Graph_Parameter,
                            .goal = 0.93f,
                            .x_index = (i32)sample,
                            .param = fng->param,
                            .value = lerp(state.ps.min[fng->param], state.ps.max[fng->param], (f32)sample / (NumGraphSamples - 1)),
                            .param_of_fng = fng->param,
                            .generation = state.generation,
                        });
                    }
                }
            }
        }
    }

    if ((type & Graph_Parameter) == Graph_Parameter) {
        // Skillsets over parameter (that is the one that just changed)
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            for (isize i = 1; i < buf_len(sfi->graphs); i++) {
                FnGraph *fng = &state.graphs[sfi->graphs[i]];
                if (fng->param == param) {
                    for (isize sample = 0; sample < NumGraphSamples; sample++) {
                        buf_push(*work, (CalcWork) {
                            .sfi = sfi,
                            .type = type,
                            .bound = value,
                            .x_index = (i32)sample,
                            .param = param,
                            .value = lerp(state.ps.min[param], state.ps.max[param], (f32)sample / (NumGraphSamples - 1)),
                            .param_of_fng = param,
                            .generation = state.generation,
                        });
                    }
                }
            }
        }
    }
}

void submit_work(CalcWork work[], u32 generation)
{
    // Loading queue.read (but not queue.write) is a race condition. The
    // consequence is always that max_to_submit is smaller than it could be,
    // which is fine
    isize max_to_submit = (array_length(low_prio_work_queue.entries) - 1) - (low_prio_work_queue.write - low_prio_work_queue.read);
    assert(0 <= max_to_submit && max_to_submit < 4096);

    isize N = mins(buf_len(work), max_to_submit);
    isize n = 0;
    for (isize i = 0; i < N; i++) {
        if (work[i].generation == generation) {
            low_prio_work_queue.entries[(low_prio_work_queue.write + i) & WorkQueueMask] = work[i];
            n++;
        }
    }

    wag_tail();
    low_prio_work_queue.write += n;
    thread_notify();

    debug_counters.requested += n;

    memmove(work, work + n, sizeof(CalcWork) * (buf_len(work) - n));
    buf_set_len(work, -n);
}
