
/*[[[cog
import cog
import numpy as np

def lerp(a, b, t):
    return (1 - t)*a + b*t

def dump(name, xs):
    cog.outl("static const f32 %s[%s] = {" % (name, len(xs)))
    for x in xs:
        cog.outl("    %sf," % x.astype(np.float32))
    cog.outl("};")

linear = np.linspace(1, 0, 127)

wife_a = lerp(0.82, 0.965, 1 - linear*linear*np.sqrt(linear))
closest_to_93 = wife_a[np.argmin(np.abs(wife_a - 0.93))]
wife_a -= 0.965
wife_a *= (0.93 - 0.965) / (closest_to_93 - 0.965)
wife_a += 0.965

wife_xs = np.concatenate((wife_a, [0.98]))

dump("WifeXs", wife_xs)
cog.outl(r"""enum {
    Wife930Index = %s,
    Wife965Index = %s
};""" % (np.argwhere(wife_xs == 0.93)[0,0], np.argwhere(wife_xs == 0.965)[0,0]))
]]]*/
static const f32 WifeXs[128] = {
    0.81815857f,
    0.8210548f,
    0.82391644f,
    0.8267437f,
    0.8295367f,
    0.83229554f,
    0.83502036f,
    0.8377114f,
    0.8403687f,
    0.8429924f,
    0.8455827f,
    0.84813976f,
    0.85066366f,
    0.85315454f,
    0.85561264f,
    0.85803795f,
    0.8604308f,
    0.8627912f,
    0.8651193f,
    0.8674153f,
    0.8696794f,
    0.87191164f,
    0.8741122f,
    0.87628126f,
    0.878419f,
    0.8805255f,
    0.88260096f,
    0.8846455f,
    0.8866593f,
    0.88864255f,
    0.8905953f,
    0.8925178f,
    0.89441025f,
    0.89627266f,
    0.8981053f,
    0.8999083f,
    0.90168184f,
    0.90342605f,
    0.90514106f,
    0.90682715f,
    0.9084844f,
    0.910113f,
    0.9117131f,
    0.9132849f,
    0.91482854f,
    0.91634417f,
    0.917832f,
    0.9192923f,
    0.920725f,
    0.92213047f,
    0.92350876f,
    0.9248602f,
    0.92618483f,
    0.9274829f,
    0.92875457f,
    0.93f,
    0.9312194f,
    0.932413f,
    0.9335809f,
    0.93472326f,
    0.93584037f,
    0.9369324f,
    0.9379995f,
    0.93904185f,
    0.9400597f,
    0.9410532f,
    0.9420226f,
    0.9429681f,
    0.9438898f,
    0.944788f,
    0.94566286f,
    0.9465146f,
    0.9473434f,
    0.94814956f,
    0.9489332f,
    0.9496945f,
    0.9504338f,
    0.9511512f,
    0.95184696f,
    0.9525214f,
    0.9531746f,
    0.9538068f,
    0.95441836f,
    0.95500934f,
    0.9555801f,
    0.95613086f,
    0.9566618f,
    0.9571732f,
    0.9576653f,
    0.9581384f,
    0.95859265f,
    0.95902836f,
    0.95944583f,
    0.95984524f,
    0.96022695f,
    0.96059114f,
    0.96093816f,
    0.9612682f,
    0.96158165f,
    0.9618787f,
    0.96215975f,
    0.96242505f,
    0.96267486f,
    0.9629095f,
    0.9631294f,
    0.9633348f,
    0.963526f,
    0.9637034f,
    0.9638673f,
    0.96401817f,
    0.9641562f,
    0.964282f,
    0.9643957f,
    0.9644979f,
    0.96458894f,
    0.96466935f,
    0.96473944f,
    0.96479976f,
    0.96485084f,
    0.96489316f,
    0.9649273f,
    0.96495396f,
    0.9649736f,
    0.96498716f,
    0.9649953f,
    0.9649992f,
    0.965f,
    0.98f,
};
enum {
    Wife930Index = 55,
    Wife965Index = 126
};
//[[[end]]] (checksum: 98d4593f5683998f6fe114f951055a5c)

enum {
    NumGraphSamples = array_length(WifeXs)
};

static_assert(NumGraphSamples < 256);
struct FnGraph
{
    b8 active;
    b8 have_ys;
    b8 initialised;
    b8 zoomable_once;
    b32 is_param;
    i32 param;
    i32 len;

    u8 resident[NumGraphSamples];
    u8 resident_count;
    f32 incoming_ys[NumSkillsets][NumGraphSamples];

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
    Work_Nothing,
    Work_Wife = 1,
    Work_Parameter = 2,
    Work_Effects = 4,
    Work_Skillsets = 8,
} WorkType;

typedef struct DoneQueue DoneQueue;
typedef struct CalcThread
{
    CalcInfo *info;
    ParamSet *ps;
    u32 *generation;
    DoneQueue *done;

    struct {
        usize skipped;
        usize discarded;
        usize done;
        f64 time;
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
            f32 lower_bound;
            f32 upper_bound;
            i32 param;
            f32 value;
            i32 param_of_fng;
        } parameter;
        struct {
            i32 start;
            i32 end;
        } effects;
        struct {
            b32 initialisation;
        } skillsets;
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

        ratings = calc_go_with_param(calc, &info->defaults, note_data, 0.93f, param, value);

        for (int r = 0; r < NumSkillsets; r++) {
            b32 changed = rating_floor(ratings.E[r]) != default_ratings->E[r];
            out->strong[p] |= (changed << r);
        }
    }

    // weak. they react to big changes at 96.5%, and small changes at 93%
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

        ratings = calc_go_with_param(calc, &info->defaults, note_data, 0.93f, param, value);

        out->weak[p] = out->strong[p];
        for (i32 r = 0; r < NumSkillsets; r++) {
            b32 changed = rating_floor(ratings.E[r]) != default_ratings->E[r];
            out->weak[p] |= (changed << r);
        }
    }
}

typedef struct DoneWork
{
    usize id;
    CalcWork work;
    SkillsetRatings ssr;
} DoneWork;

#if defined(_MSC_VER) && !defined(alignas)
#define alignas(n) __declspec(align(n))
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#else
#define alignas(n) _Alignas(n)
#endif

enum {
    WorkQueueSize = 4096,
    DoneQueueSize = WorkQueueSize * 4
};

typedef struct WorkQueue
{
    alignas(64) usize read;
    alignas(64) usize write;
    CalcWork entries[WorkQueueSize];
    int lock_id;
} WorkQueue;

typedef struct DoneQueue
{
    alignas(64) usize read;
    alignas(64) usize write;
    DoneWork entries[DoneQueueSize];
} DoneQueue;

// Low prio is only accessed if high prio is empty
// A generation counter is used to skip out of date work
static WorkQueue high_prio_work_queue = {0};
static WorkQueue low_prio_work_queue = {0};
static DoneQueue *done_queues = 0;

struct {
    usize requested;
    usize skipped;
    usize done;
} debug_counters = {0};

static const usize WorkQueueMask = WorkQueueSize - 1;
static const usize DoneQueueMask = DoneQueueSize - 1;
static_assert((WorkQueueSize & (WorkQueueSize - 1)) == 0);

void calculate_file_graph_force(CalcWork *work[], SimFileInfo *sfi, FnGraph *fng, u32 generation)
{
    assert(fng->is_param == false);
    for (isize i = 0; i < NumGraphSamples; i++) {
        buf_push(*work, (CalcWork) {
            .sfi = sfi,
            .type = Work_Wife,
            .wife.goal = WifeXs[i],
            .x_index = (i32)i,
            .generation = generation,
        });
    }
}

void calculate_file_graph(CalcWork *work[], SimFileInfo *sfi, FnGraph *fng, u32 generation)
{
    assert(fng->is_param == false);
    if (fng->pending_generation < generation) {
        fng->pending_generation = generation;
        for (isize i = 0; i < NumGraphSamples; i++) {
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Wife,
                .wife.goal = WifeXs[i],
                .x_index = (i32)i,
                .generation = generation,
            });
        }
    }
}

void calculate_parameter_graph_force(CalcWork *work[], SimFileInfo *sfi, FnGraph *fng, u32 generation)
{
    assert(fng->is_param == true);
    if (state.info.params[fng->param].integer == false) {
        for (isize sample = 0; sample < fng->len; sample++) {
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Parameter,
                .x_index = (i32)sample,
                .parameter.lower_bound = state.ps.min[fng->param],
                .parameter.upper_bound = state.ps.max[fng->param],
                .parameter.param = fng->param,
                .parameter.value = lerp(state.ps.min[fng->param], state.ps.max[fng->param], (f32)sample / ((f32)fng->len - 1.0f)),
                .parameter.param_of_fng = fng->param,
                .generation = generation,
            });
        }
    } else {
        for (isize sample = 0; sample < fng->len; sample++) {
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Parameter,
                .x_index = (i32)sample,
                .parameter.lower_bound = state.ps.min[fng->param],
                .parameter.upper_bound = state.ps.max[fng->param],
                .parameter.param = fng->param,
                .parameter.value = (f32)sample,
                .parameter.param_of_fng = fng->param,
                .generation = generation,
            });
        }
    }
}

void calculate_parameter_graph(CalcWork *work[], SimFileInfo *sfi, FnGraph *fng, u32 generation)
{
    assert(fng->is_param == true);
    if (fng->pending_generation < generation) {
        fng->pending_generation = generation;
        calculate_parameter_graph_force(work, sfi, fng, generation);
    }
}

void calculate_file_graphs(CalcWork *work[], SimFileInfo *sfi, u32 generation)
{
    // Skillsets over wife
    calculate_file_graph(work, sfi, &state.graphs[sfi->graphs[0]], generation);

    // Skillsets over parameter
    for (isize i = buf_len(sfi->graphs) - 1; i >= 1; i--) {
        FnGraph *fng = &state.graphs[sfi->graphs[i]];
        calculate_parameter_graph(work, sfi, fng, generation);
    }
}

void calculate_graphs_in_background(CalcWork *work[], WorkType type, i32 param, u32 generation)
{
    if (type == Work_Wife) {
        // 93 and 96.5, for the files list
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Wife,
                .x_index = Wife930Index,
                .wife.goal = WifeXs[Wife930Index],
                .generation = generation,
            });
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Wife,
                .x_index = Wife965Index,
                .wife.goal = WifeXs[Wife965Index],
                .generation = generation,
            });
        }

        // Skillsets over wife
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            if (sfi->open == false) {
                for (isize i = 0; i < NumGraphSamples; i++) {
                    if (i != Wife930Index && i < Wife965Index) {
                        buf_push(*work, (CalcWork) {
                            .sfi = sfi,
                            .type = Work_Wife,
                            .wife.goal = WifeXs[i],
                            .x_index = (i32)i,
                            .generation = generation,
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
                        calculate_parameter_graph(work, sfi, fng, generation);
                    }
                }
            }
        }
    }

    if (type == Work_Parameter) {
        // Skillsets over parameter (that is the one that just changed)
        for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
            for (isize i = 1; i < buf_len(sfi->graphs); i++) {
                if (sfi->open == false) {
                    FnGraph *fng = &state.graphs[sfi->graphs[i]];
                    if (fng->param == param) {
                        calculate_parameter_graph(work, sfi, fng, generation);
                    }
                }
            }
        }
    }
}

void calculate_skillsets(CalcWork *work[], SimFileInfo *sfi, b32 initialisation, u32 generation)
{
    buf_push(*work, (CalcWork) {
        .sfi = sfi,
        .type = Work_Skillsets,
        .x_index = Wife930Index,
        .skillsets.initialisation = initialisation,
        .generation = generation,
    });
    buf_push(*work, (CalcWork) {
        .sfi = sfi,
        .type = Work_Skillsets,
        .x_index = Wife965Index,
        .skillsets.initialisation = initialisation,
        .generation = generation,
    });
}

void calculate_skillsets_in_background(CalcWork *work[], u32 generation)
{
    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
        calculate_skillsets(work, sfi, false, generation);
    }
}

void calculate_effects(CalcWork *work[], CalcInfo *info, SimFileInfo *sfi, u32 generation)
{
    i32 stride = 8;
    if (sfi->effects_generation != generation && sfi->num_effects_computed < info->num_params) {
        sfi->effects_generation = generation;
        for (i32 i = 0; i < info->num_params; i += stride) {
            buf_push(*work, (CalcWork) {
                .sfi = sfi,
                .type = Work_Effects,
                .effects.start = i,
                .effects.end =  (i32)mins(i + stride, info->num_params),
                .generation = generation,
            });
        }
    }
}

b32 get_done_work(DoneWork *out)
{
    static isize next_queue_to_read = 0;

    isize q = 0;
    isize i = 0;
    for (; i < buf_len(done_queues); i++) {
        q = next_queue_to_read++;
        if (next_queue_to_read == buf_len(done_queues)) {
            next_queue_to_read = 0;
        }

        if (done_queues[q].read != done_queues[q].write) {
            break;
        }
    }
    if (i == buf_len(done_queues)) {
        return false;
    }

    *out = done_queues[q].entries[done_queues[q].read & DoneQueueMask];
    wag_tail();
    done_queues[q].read++;

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
    u32 generation = *ct->generation;

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
    DoneQueue *done = ct->done;

    while (true) {
        CalcWork work = {0};
        while (get_work(ct, &high_prio_work_queue, &work) || get_work(ct, &low_prio_work_queue, &work)) {
            SkillsetRatings ssr = {0};
            u64 then = 0;
            u64 now = 0;
            switch (work.type) {
                case Work_Parameter: {
                    if (work.parameter.lower_bound != ps->min[work.parameter.param]
                     || work.parameter.upper_bound != ps->max[work.parameter.param]) {
                         ct->debug_counters.skipped++;
                        continue;
                    }
                    then = stm_now();
                    ssr = calc_go_with_param(&calc, ps, work.sfi->notes, 0.93f, work.parameter.param, work.parameter.value);
                    now = stm_now();
                } break;
                case Work_Wife: {
                    then = stm_now();
                    ssr = calc_go(&calc, ps, work.sfi->notes, work.wife.goal);
                    now = stm_now();
                } break;
                case Work_Effects: {
                    for (i32 i = work.effects.start; i < work.effects.end; i++) {
                        // nb. modifies sfi->effects from this thread; it is otherwise read-only
                        calculate_effect_for_param(ct->info, &calc, work.sfi->notes, i, &work.sfi->default_ratings, &work.sfi->effects);
                    }
                } break;
                case Work_Skillsets: {
                    then = stm_now();
                    ssr = calc_go(&calc, ps, work.sfi->notes, WifeXs[work.x_index]);
                    now = stm_now();
                } break;
                default: assert_unreachable();
            }

            done->entries[done->write & DoneQueueMask] = (DoneWork) {
                .work = work,
                .ssr = ssr
            };
            wag_tail();
            done->write++;

            ct->debug_counters.done++;

            if (now - then > 0 && work.sfi->notes_len > 0) {
                ct->debug_counters.time = ct->debug_counters.time*0.99 + 0.01*(stm_ms(now - then) / work.sfi->notes_len);
            }
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
            q->entries[(q->write + n) & WorkQueueMask] = work[i];
            n++;
        }
    }

    wag_tail();
    q->write += n;
    thread_notify();

    debug_counters.requested += n;

    memmove(work, work + N, sizeof(CalcWork) * (buf_len(work) - N));
    buf_set_len(work, buf_len(work) - N);
}

void finish_work()
{
    push_allocator(scratch);
    FnGraph **updated_fngs = 0;

    DoneWork done = {0};
    while (get_done_work(&done)) {
        SimFileInfo *sfi = done.work.sfi;
        FnGraph *fng = 0;
        switch (done.work.type) {
            case Work_Wife: {
                fng = &state.graphs[done.work.sfi->graphs[0]];
            } break;
            case Work_Parameter: {
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
            } break;
            case Work_Effects: {
                sfi->num_effects_computed += done.work.effects.end - done.work.effects.start;
                continue;
            } break;
            case Work_Skillsets: {
                fng = &state.graphs[done.work.sfi->graphs[0]];

                for (isize ss = 0; ss < NumSkillsets; ss++) {
                    fng->ys[ss][done.work.x_index] = done.ssr.E[ss];
                }

                if (done.work.x_index == Wife930Index) {
                    sfi->aa_rating = done.ssr.overall;
                    for (isize ss = 0; ss < NumSkillsets; ss++) {
                        sfi->display_skillsets[ss] = 0.9f <= (done.ssr.E[ss] / done.ssr.overall);
                    }

                    if (done.work.skillsets.initialisation) {
                        sfi->default_ratings = done.ssr;
                        for (isize ss = 0; ss < NumSkillsets; ss++) {
                            sfi->default_ratings.E[ss] = rating_floor(sfi->default_ratings.E[ss]);
                            sfi->selected_skillsets[ss] = sfi->display_skillsets[ss];
                        }
                    }
                } else {
                    assert(done.work.x_index == Wife965Index);
                    sfi->max_rating = done.ssr.overall;
                }
                continue;
            } break;
            default: assert_unreachable();
        }

        if (fng->generation > done.work.generation) {
            continue;
        }
        if (fng->generation < done.work.generation) {
            memset(fng->resident, 0, sizeof(fng->resident));
            fng->resident_count = 0;
        }

        fng->generation = done.work.generation;
        assert(done.work.x_index < NumGraphSamples);
        if (fng->resident[done.work.x_index] == 0) {
            fng->resident_count++;
        }
        fng->resident[done.work.x_index] = 1;
        assert(fng->resident_count <= array_length(fng->resident));

        for (isize ss = 0; ss < NumSkillsets; ss++) {
            if (done.work.type == Work_Parameter) {
                fng->xs[done.work.x_index] = done.work.parameter.value;
            }
            fng->incoming_ys[ss][done.work.x_index] = done.ssr.E[ss];
        }

        if (fng->is_param == false) {
            sfi->aa_rating = fng->ys[0][Wife930Index];
            sfi->max_rating = fng->ys[0][Wife965Index];

            if (fng->resident_count == array_length(fng->resident)) {
                for (isize ss = 1; ss < NumSkillsets; ss++) {
                    sfi->display_skillsets[ss] = (0.9f <= (fng->incoming_ys[ss][Wife930Index] / fng->incoming_ys[0][Wife930Index]));
                }
            }
        }

        isize fungi = 0;
        for (; fungi < buf_len(updated_fngs); fungi++) {
            if (updated_fngs[fungi] == fng) {
                break;
            }
        }
        if (fungi == buf_len(updated_fngs)) {
            buf_push(updated_fngs, fng);
        }
    }

    for (isize fungi = 0; fungi < buf_len(updated_fngs); fungi++) {
        FnGraph *fng = updated_fngs[fungi];
        if (fng->resident_count != fng->len) {
            // 🧙‍♂️
            u8 cumsum[NumGraphSamples] = {0};
            u8 sorted[NumGraphSamples] = {0};
            for (isize i = 1; i < fng->len; i++) {
                cumsum[i] = cumsum[i - 1] + fng->resident[i - 1];
            }
            for (isize i = 0; i < fng->len; i++) {
                sorted[cumsum[i]] = (u8)i;
            }

            f32 a = fng->have_ys ? 0.7f : 0.0f;
            f32 b = 1.0f - a;
            f32 min_y = 100.0f;
            f32 rel_min_y = 100.0f;
            f32 max_y = 0.0f;
            for (isize ss = 0; ss < NumSkillsets; ss++) {
                for (isize r = 0; r < fng->resident_count - 1; r++) {
                    if (fng->ys[ss][sorted[r]] == fng->incoming_ys[ss][sorted[r]]
                     && fng->ys[ss][sorted[r+1]] == fng->incoming_ys[ss][sorted[r+1]]) {
                        continue;
                    }

                    f32 t_a = fng->incoming_ys[ss][sorted[r]];
                    f32 t_b = fng->incoming_ys[ss][sorted[r+1]];
                    f32 inc = 1.0f / (f32)(sorted[r+1] - sorted[r]);
                    f32 t = 0.0f;
                    for (isize i = sorted[r]; i < sorted[r+1]; i++) {
                        fng->ys[ss][i] = a*fng->ys[ss][i] + b*lerp(t_a, t_b, t);
                        fng->relative_ys[ss][i] = fng->ys[ss][i] / fng->ys[0][i];
                        t += inc;
                    }
                }
                for (isize i = 0; i < fng->len; i++) {
                    if (fng->ys[ss][i] < min_y) {
                        min_y = fng->ys[ss][i];
                    }
                    if (fng->relative_ys[ss][i] < rel_min_y) {
                        rel_min_y = fng->relative_ys[ss][i];
                    }
                    if (fng->ys[ss][i] > max_y) {
                        max_y = fng->ys[ss][i];
                    }
                }
                fng->ys[ss][Wife965Index + 1] = fng->ys[ss][Wife965Index];
                fng->relative_ys[ss][Wife965Index + 1] = fng->relative_ys[ss][Wife965Index];
            }
            fng->min = (min_y == 100.f) ? 0.f : min_y;
            fng->relative_min = (rel_min_y == 100.f) ? 0.f : rel_min_y;
            fng->max = (max_y == 0.f) ? 40.f : max_y;
            fng->have_ys = true;
        } else {
            f32 min_y = 100.0f;
            f32 rel_min_y = 100.0f;
            f32 max_y = 0.0f;
            for (isize ss = 0; ss < NumSkillsets; ss++) {
                for (isize i = 0; i < fng->len; i++) {
                    fng->ys[ss][i] = fng->incoming_ys[ss][i];
                    fng->relative_ys[ss][i] = fng->ys[ss][i] / fng->ys[0][i];

                    if (fng->ys[ss][i] < min_y) {
                        min_y = fng->ys[ss][i];
                    }
                    if (fng->relative_ys[ss][i] < rel_min_y) {
                        rel_min_y = fng->relative_ys[ss][i];
                    }
                    if (fng->ys[ss][i] > max_y) {
                        max_y = fng->ys[ss][i];
                    }
                }
            }
            fng->min = (min_y == 100.f) ? 0.f : min_y;
            fng->relative_min = (rel_min_y == 100.f) ? 0.f : rel_min_y;
            fng->max = (max_y == 0.f) ? 40.f : max_y;
            fng->initialised = true;
        }
    }

    pop_allocator();
}
