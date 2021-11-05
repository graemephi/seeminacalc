// cog -rc graphs.c
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

typedef struct OptimizationDelta {
    f32 delta_squared;
    i32 skillset;
} OptimizationDelta;

static_assert(NumGraphSamples < 256);
struct FnGraph
{
    b8 active;
    b8 have_ys;
    b8 initialised;
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

    OptimizationDelta *deltas;
};

typedef enum {
    Work_Nothing,
    Work_Wife,
    Work_Parameter,
    Work_ParameterLoss,
    Work_Effects,
    Work_Skillsets,
    Work_Target,
    Work_DebugGraphs,
    Work_Participation,
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

typedef struct ParameterLossWork
{
    i32 sample;
    i32 param;
    f32 value;
    f32 goal;
    f32 msd;
    f32 x;
} ParameterLossWork;

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
        ParameterLossWork parameter_loss;
    };
    u32 generation;
} CalcWork;

static f32 rating_floor(f32 v)
{
    return (f32)((i32)(v * 100.0f)) / 100.f;
}

static SkillsetRatings rating_floor_skillsets(SkillsetRatings ssr)
{
    for (isize ss = 0; ss < array_length(ssr.E); ss++) {
        ssr.E[ss] = rating_floor(ssr.E[ss]);
    }
    return ssr;
}

void calculate_effect_for_param(CalcInfo *info, SeeCalc *calc, ParamSet *ps, NoteData *note_data, f32 rate, i32 param, EffectMasks *out)
{
    SkillsetRatings default_ratings = rating_floor_skillsets(calc_go_with_param(calc, ps, note_data, 0.93f, 0, rate));
    assert(info->params[param].max >= info->params[param].min);

    SkillsetRatings ratings = calc_go_with_rate_and_param(calc, ps, note_data, 0.93f, rate, param, info->params[param].max);
    for (i32 ss = 0; ss < NumSkillsets; ss++) {
        b32 changed = rating_floor(ratings.E[ss]) != default_ratings.E[ss];
        out->masks[param] |= (changed << ss);
    }

    if (out->masks[param] == 0) {
        ratings = calc_go_with_rate_and_param(calc, ps, note_data, 0.93f, rate, param, info->params[param].min);
        for (i32 ss = 0; ss < NumSkillsets; ss++) {
            b32 changed = rating_floor(ratings.E[ss]) != default_ratings.E[ss];
            out->masks[param] |= (changed << ss);
        }
    }
}

typedef struct DoneWork
{
    usize id;
    CalcWork work;
    union {
        SkillsetRatings ssr;
        DebugInfo debug_info;
        b32 participating;
    };
} DoneWork;

enum {
    WorkQueueSize = 4096,
    DoneQueueSize = WorkQueueSize * 4
};

typedef struct WorkQueue
{
    alignas(64) usize read;
    alignas(64) usize write;
    CalcWork entries[WorkQueueSize];
    Lock *lock;
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
static BadSem calc_sem;

struct {
    usize requested;
    usize skipped;
    usize done;
} debug_counters = {0};

static const usize WorkQueueMask = WorkQueueSize - 1;
static const usize DoneQueueMask = DoneQueueSize - 1;
static_assert((WorkQueueSize & (WorkQueueSize - 1)) == 0);

void calculate_file_graph_force(CalcWork *work[], SimFileInfo *sfi, u32 generation)
{
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
    if (fng->pending_generation != generation) {
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
    if (fng->pending_generation != generation) {
        fng->pending_generation = generation;
        calculate_parameter_graph_force(work, sfi, fng, generation);
    }
}

void calculate_parameter_loss(CalcWork *work[], SimFileInfo *sfi, ParameterLossWork pmw, u32 generation)
{
    assert(sfi->target.want_msd);
    buf_push(*work, (CalcWork) {
        .sfi = sfi,
        .type = Work_ParameterLoss,
        .parameter_loss = pmw,
        .generation = generation,
    });
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
    if (sfi->skillsets_generation != generation) {
        sfi->skillsets_generation = generation;
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
}

void calculate_skillsets_in_background(CalcWork *work[], u32 generation)
{
    for (SimFileInfo *sfi = state.files; sfi != buf_end(state.files); sfi++) {
        if (sfi_visible_in_right_pane(sfi)) {
            calculate_skillsets(work, sfi, false, generation);
        }
    }
}

void calculate_effects(CalcWork *work[], CalcInfo *info, SimFileInfo *sfi,  u32 generation)
{
    if (sfi->effects_generation != generation && sfi->num_effects_computed < info->num_params) {
        sfi->effects_generation = generation;
        for (i32 i = 0; i < info->num_params; i++) {
            b32 already_positive = sfi->effects.masks[i];
            if (!already_positive) {
                buf_push(*work, (CalcWork) {
                    .sfi = sfi,
                    .type = Work_Effects,
                    .effects.start = i,
                    .effects.end = i + 1,
                    .generation = generation,
                });
            }
        }
    }
}

void calculate_debug_graphs(CalcWork *work[], SimFileInfo *sfi, u32 generation)
{
    if (sfi->debug_generation != generation) {
        sfi->debug_generation = generation;
        sfi->debug_generation_is_current = false;
        buf_push(*work, (CalcWork) {
            .sfi = sfi,
            .type = Work_DebugGraphs,
            .generation = generation,
        });
    }
}

void calculate_file_opt_participation(CalcWork *work[], SimFileInfo *sfi, u32 generation)
{
    if (sfi->opt_generation != generation) {
        sfi->opt_generation = generation;
        buf_push(*work, (CalcWork) {
            .sfi = sfi,
            .type = Work_Participation,
            .generation = generation,
        });
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

    memory_barrier();
    *out = done_queues[q].entries[done_queues[q].read & DoneQueueMask];
    memory_barrier();
    done_queues[q].read++;

    return true;
}

b32 get_work(CalcThread *ct, WorkQueue *q, CalcWork *out)
{
    b32 ok = false;

    if (q->read == q->write) {
        return false;
    }

    lock(q->lock);

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

    unlock(q->lock);

    ct->debug_counters.skipped += r - read;
    return ok;
}

i32 calc_thread(void *userdata)
{
    CalcThread *ct = userdata;
    SeeCalc calc = calc_init(ct->info);
    ParamSet *ps = ct->ps;
    DoneQueue *done = ct->done;

    Stack participation_stack = stack_make(malloc(8*1024*1024), 8*1024*1024);
    push_allocator(&participation_stack);

    while (true) {
        CalcWork work = {0};
        while (get_work(ct, &high_prio_work_queue, &work) || get_work(ct, &low_prio_work_queue, &work)) {
            DoneWork result = (DoneWork) { .work = work };
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
                    result.ssr = calc_go_with_param(&calc, ps, work.sfi->notes, 0.93f, work.parameter.param, work.parameter.value);
                    now = stm_now();
                } break;
                case Work_ParameterLoss: {
                    then = stm_now();
                    result.ssr = calc_go_with_rate_and_param(&calc, ps, work.sfi->notes, work.parameter_loss.goal, work.sfi->target.rate, work.parameter_loss.param, work.parameter_loss.value);
                    now = stm_now();
                } break;
                case Work_Wife: {
                    then = stm_now();
                    result.ssr = calc_go_with_param(&calc, ps, work.sfi->notes, work.wife.goal, 0, work.sfi->target.rate);
                    now = stm_now();
                } break;
                case Work_Effects: {
                    for (i32 i = work.effects.start; i < work.effects.end; i++) {
                        // nb. modifies sfi->effects from this thread; it is otherwise read-only
                        calculate_effect_for_param(ct->info, &calc, ps, work.sfi->notes, work.sfi->target.rate, i, &work.sfi->effects);
                    }
                } break;
                case Work_Skillsets: {
                    then = stm_now();
                    result.ssr = calc_go_with_param(&calc, ps, work.sfi->notes, WifeXs[work.x_index], 0, work.sfi->target.rate);
                    now = stm_now();
                } break;
                case Work_DebugGraphs: {
                    then = stm_now();
                    result.debug_info = calc_go_debuginfo(&calc, ps, work.sfi->notes, work.sfi->target.rate);
                    now = stm_now();
                } break;
                case Work_Participation: {
                    stack_reset(&participation_stack);
                    ParamSet p_ps = copy_param_set(ps);
                    SkillsetRatings ssr_a = calc_go_with_param(&calc, &p_ps, work.sfi->notes, 0.93f, 0, work.sfi->target.rate);
                    ssr_a = rating_floor_skillsets(ssr_a);

                    for (isize i = 0; i < p_ps.num_params; i++) {
                        if (state.opt_cfg.enabled[i]) {
                            // Lerp instead of going to fully max because setting many parameters fully max
                            // can blow out the returned values into useless ranges, i.e. where we don't
                            // want the optimizer to be searching anyway. Hard to say how much we can pull
                            // back on this without introducing too much bias
                            p_ps.params[i] = lerp(p_ps.params[i], p_ps.max[i], 0.25f);
                        }
                    }
                    SkillsetRatings ssr_b = calc_go_with_param(&calc, &p_ps, work.sfi->notes, 0.93f, 0, work.sfi->target.rate);
                    ssr_b = rating_floor_skillsets(ssr_b);

                    b32 participating = ssr_a.E[work.sfi->target.skillset] != ssr_b.E[work.sfi->target.skillset];

                    if (participating == false) {
                        // Again but min, just in case
                        for (isize i = 0; i < p_ps.num_params; i++) {
                            if (state.opt_cfg.enabled[i]) {
                                p_ps.params[i] = p_ps.min[i];
                            }
                        }
                        SkillsetRatings ssr_c = calc_go_with_param(&calc, &p_ps, work.sfi->notes, 0.93f, 0, work.sfi->target.rate);
                        ssr_c = rating_floor_skillsets(ssr_c);

                        participating = ssr_a.overall - ssr_c.overall
                                     || ssr_a.E[work.sfi->target.skillset] != ssr_c.E[work.sfi->target.skillset];
                    }

                    result.participating = participating;
                } break;
                default: assert_unreachable();
            }

            done->entries[done->write & DoneQueueMask] = result;
            memory_barrier();
            done->write++;

            ct->debug_counters.done++;

            if (now - then > 0 && work.sfi->n_rows > 0) {
                ct->debug_counters.time = ct->debug_counters.time*0.99 + 0.01*(stm_ms(now - then) / work.sfi->n_rows);
            }
        }

        thread_wait(calc_sem);
    }

    return 0;
}

void submit_work(WorkQueue *q, CalcWork work[], u32 generation)
{
    // Loading queue.read (but not queue.write) is a race condition. The
    // consequence is always that max_to_submit is smaller than it could be,
    // which is fine
    isize max_to_submit = (array_length(q->entries) - 1) - (q->write - q->read);
    memory_barrier();
    assert(0 <= max_to_submit && max_to_submit < 4096);

    isize N = mins(buf_len(work), max_to_submit);
    isize n = 0;
    for (isize i = 0; i < N; i++) {
        if (work[i].generation == generation) {
            q->entries[(q->write + n) & WorkQueueMask] = work[i];
            n++;
        }
    }

    memory_barrier();
    q->write += n;
    thread_notify(calc_sem);

    debug_counters.requested += n;

    buf_remove_first_n(work, N);
}

void finish_work(void)
{
    push_allocator(scratch);
    FnGraph **updated_fngs = 0;
    buf_reserve(updated_fngs, 128);
    pop_allocator();

    b32 targets_updated = false;

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
                    // fng removed between now and work submission
                    continue;
                }
            } break;
            case Work_ParameterLoss: {
                if (done.work.generation != state.generation) {
                    continue;
                }
                void optimizer_skulduggery(SimFileInfo *sfi, ParameterLossWork work, SkillsetRatings ssr);
                optimizer_skulduggery(sfi, done.work.parameter_loss, done.ssr);

                if (done.work.parameter_loss.param == 0) {
                    sfi->target.got_msd = done.ssr.E[sfi->target.skillset];
                    sfi->target.delta = sfi->target.got_msd - sfi->target.want_msd;
                    sfi->aa_rating = rating_floor_skillsets(done.ssr);
                    targets_updated = true;
                }
                continue;
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
                    sfi->aa_rating = rating_floor_skillsets(done.ssr);
                    for (isize ss = 0; ss < NumSkillsets; ss++) {
                        sfi->display_skillsets[ss] = 0.9f <= (done.ssr.E[ss] / done.ssr.overall);
                    }

                    if (done.work.skillsets.initialisation) {
                        sfi->default_ratings = rating_floor_skillsets(done.ssr);
                        for (isize ss = 0; ss < NumSkillsets; ss++) {
                            sfi->selected_skillsets[ss] = sfi->display_skillsets[ss];
                        }
                    }
                    if (sfi->target.want_msd == -1.0f) {
                        isize max_ss = 1;
                        f32 max_msd = sfi->aa_rating.E[1];
                        for (isize ss = 2; ss < NumSkillsets; ss++) {
                            if (sfi->aa_rating.E[ss] > max_msd) {
                                max_msd = sfi->aa_rating.E[ss];
                                max_ss = ss;
                            }
                        }
                        sfi->target.want_msd = max_msd;
                        sfi->target.skillset = max_ss;
                        sfi->target.weight = 1.0f;
                    }
                    sfi->target.got_msd = done.ssr.E[sfi->target.skillset];
                    sfi->target.delta = done.ssr.E[sfi->target.skillset] - sfi->target.want_msd;
                    targets_updated = true;
                } else {
                    assert(done.work.x_index == Wife965Index);
                    sfi->max_rating = rating_floor_skillsets(done.ssr);
                }
                continue;
            } break;
            case Work_DebugGraphs: {
                if (done.work.generation >= done.work.sfi->debug_generation) {
                    assert(done.work.generation == done.work.sfi->debug_generation);
                    if (done.work.sfi->debug_info.buffers) {
                        debuginfo_free(&done.work.sfi->debug_info);
                    }
                    done.work.sfi->debug_info = done.debug_info;
                    done.work.sfi->debug_generation_is_current = true;
                } else {
                    debuginfo_free(&done.debug_info);
                }
                continue;
            } break;
            case Work_Participation: {
                if (done.work.generation >= done.work.sfi->opt_generation) {
                    assert(done.work.generation == done.work.sfi->opt_generation);
                    done.work.sfi->opt_generation = done.work.generation;
                    done.work.sfi->opt_participating = done.participating;

                    assert(state.opt_pending_participating > 0);
                    state.opt_pending_participating--;
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
            fng->resident[done.work.x_index] = 1;
        }
        assert(fng->resident_count <= array_length(fng->resident));

        if (done.work.type == Work_Parameter) {
            fng->xs[done.work.x_index] = done.work.parameter.value;
        }

        for (isize ss = 0; ss < NumSkillsets; ss++) {
            fng->incoming_ys[ss][done.work.x_index] = done.ssr.E[ss];
        }

        if (fng->is_param == false) {
            if (done.work.x_index == Wife930Index) {
                for (isize ss = 0; ss < NumSkillsets; ss++) {
                    sfi->aa_rating.E[ss] = rating_floor(fng->ys[ss][Wife930Index]);
                }
            }
            if (done.work.x_index == Wife965Index) {
                for (isize ss = 0; ss < NumSkillsets; ss++) {
                    sfi->max_rating.E[ss] = rating_floor(fng->ys[ss][Wife965Index]);
                }
            }
            if (fng->resident_count == array_length(fng->resident)) {
                for (isize ss = 1; ss < NumSkillsets; ss++) {
                    sfi->display_skillsets[ss] = (0.9f <= (fng->incoming_ys[ss][Wife930Index] / fng->incoming_ys[ss][Wife930Index]));
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

    if (targets_updated) {
        f32 delta_sum[NumSkillsets] = {0};
        f32 max_error = 0.0f;
        f32 min_error = 40.0f;
        i32 targets[NumSkillsets] = {0};
        for (isize i = 0; i < buf_len(state.files); i++) {
            if (state.files[i].target.got_msd != 0.0f && state.files[i].opt_participating) {
                // In principle, the target skillset could be 0 (overall), but we use the first index
                // for the error over all skillsets
                assert(state.files[i].target.skillset != 0);
                assert(state.files[i].target.skillset < NumSkillsets);
                delta_sum[0] += fabsf(state.files[i].target.delta);
                targets[0]++;
                delta_sum[state.files[i].target.skillset] += fabsf(state.files[i].target.delta);
                targets[state.files[i].target.skillset]++;
                max_error = max(max_error, state.files[i].target.delta);
                min_error = min(min_error, state.files[i].target.delta);
            }
        }
        for (isize i = 0; i < NumSkillsets; i++) {
            state.target.average_delta.E[i] = delta_sum[i] / (f32)targets[i];
            state.target.min_delta = min_error;
            state.target.max_delta = max_error;
        }
    }
}
