static const f32 H = 0.0001f;
static const f32 StepSize = 0.001f;
static const f32 MDecay = 0.9f;
static const f32 VDecay = 0.999f;
static const i32 SampleBatchSize = 16;
static const i32 ParameterBatchSize = 64;

enum {
    Param_None = -1
};

i32 int_cmp(i32 const *a, i32 const *b)
{
    return (*a < *b) ? -1
         : (*a > *b) ?  1
         : 0;
}

void shuffle(i32 arr[], i32 head[])
{
    qsort(arr, buf_len(arr), sizeof(i32), int_cmp);
    for (i32 i = 0; i < buf_len(head); i++) {
        i32 idx = head[i];
        arr[idx] = i;
        arr[i] = idx;
    }
    isize len = buf_len(arr);
    for (isize i = buf_len(head); i < len; i++) {
        isize idx = i + rngu(len - i);
        i32 v = arr[i];
        arr[i] = arr[idx];
        arr[idx] = v;
    }
}

typedef struct {
    i32 sample;
    i32 param;
    f32 value;
} OptimizationRequest;

typedef struct {
    f32 *x;
    f32 *v;
    f32 *m;
    struct {
        f32 at_x;
        f32 prev;
        f32 *at_xh;
    } loss;
    struct {
        i32 *samples;
        i32 *parameters;
    } active;
    struct {
        i32 submitted;
        i32 completed;
    } requests;
    struct {
        i32 sample;
        i32 parameter;
    } batch_size;
    struct {
        f32 position;
        f32 strength;
        f32 forgiveness;
    } barrier;
    i32 *focus;
    i32 n_params;
    i32 n_samples;
    i32 iter;
} OptimizationContext;

OptimizationContext optimize(i32 n_params, f32 *initial_x, i32 n_samples, f32 initial_barrier)
{
    OptimizationContext result = {
        .n_params = n_params,
        .n_samples = n_samples,
        .batch_size.sample = (i32)clamps(0, n_samples, SampleBatchSize),
        .batch_size.parameter = (i32)clamps(0, n_params, ParameterBatchSize),
        .barrier.position = is_finite(initial_barrier) ? initial_barrier : 100.0f,
        .barrier.strength = 1.0f,
        .barrier.forgiveness = 0.05f
    };
    buf_pushn(result.x, n_params);
    buf_pushn(result.v, n_params);
    buf_pushn(result.m, n_params);
    buf_pushn(result.active.samples, n_samples);
    buf_pushn(result.active.parameters, n_params);
    for (isize i = 0; i < n_params; i++) {
        result.x[i] = initial_x[i];
        result.active.parameters[i] = (i32)i;
    }
    for (isize i = 0; i < n_samples; i++) {
        result.active.samples[i] = (i32)i;
    }
    buf_reserve(result.focus, 8);
    buf_pushn(result.loss.at_xh, n_params);
    return result;
}

f32 log_barrier(f32 x, f32 a, f32 t)
{
    return -(1.0f/a)*(logf(1.0f - a*(x - t)) - logf(1 + a*t));
}

f32 loss(f32 delta, f32 barrier_strength, f32 barrier_position)
{
    f32 squared_error = delta*delta;
    f32 positive_penalty = delta < 0.0f ? 0.0f : log_barrier(delta, barrier_strength, barrier_position);
    return squared_error + positive_penalty;
}

void opt_push_evaluation(OptimizationContext *opt, OptimizationRequest req, f32 sample_delta)
{
    assert(sample_delta == sample_delta);
    if (req.param == Param_None) {
        opt->loss.at_x += loss(sample_delta, opt->barrier.strength, opt->barrier.position + opt->barrier.forgiveness);
    } else {
        assert(req.param >= 0 && req.param < opt->n_params);
        opt->loss.at_xh[req.param] += loss(sample_delta, opt->barrier.strength, opt->barrier.position + opt->barrier.forgiveness);
    }

    opt->requests.completed++;
}

void opt_focus(OptimizationContext *opt, i32 sample)
{
    assert(sample >= 0 && sample < opt->n_samples);
    for (isize i = 0; i < buf_len(opt->focus); i++) {
        if (opt->focus[i] == sample) {
            return;
        }
    }
    buf_push(opt->focus, sample);
}

OptimizationRequest *opt_pump(OptimizationContext *opt)
{
    push_allocator(scratch);
    OptimizationRequest *result = 0;
    if (opt->requests.submitted == opt->requests.completed) {
        if (opt->requests.submitted > 0) {
            for (isize i = 0; i < buf_len(opt->x); i++) {
                // ADAM: A Method For Stochastic Optimization (2015)
                f32 g = (opt->loss.at_xh[i] == 0.0f) ? 0.0f : (opt->loss.at_xh[i] - opt->loss.at_x) / H;
                opt->m[i] = lerp(g, opt->m[i], MDecay);
                opt->v[i] = lerp(g*g, opt->v[i], VDecay);
                f32 m = opt->m[i] / (1.0f - powf(MDecay, (f32)opt->iter));
                f32 v = opt->v[i] / (1.0f - powf(VDecay, (f32)opt->iter));
                opt->x[i] = opt->x[i] - StepSize * m / (sqrtf(v) + 1e-8f);
                opt->loss.at_xh[i] = 0.0f;
            }
        }

        opt->loss.at_x = 0.0f;
        opt->iter++;
        opt->barrier.position *= 0.999f;
        if (opt->barrier.strength < 10.0f) {
            opt->barrier.strength *= 1.001f;
        }

        shuffle(opt->active.samples, opt->focus);
        shuffle(opt->active.parameters, 0);

        for (isize i = 0; i < opt->batch_size.sample; i++) {
            i32 s = opt->active.samples[i];
            buf_push(result, (OptimizationRequest) {
                .sample = s,
                .param = Param_None,
            });
            for (i32 j = 0; j < opt->batch_size.parameter; j++) {
                i32 p = opt->active.parameters[j];
                buf_push(result, (OptimizationRequest) {
                    .sample = s,
                    .param = p,
                    .value = opt->x[p] + H
                });
            }
        }

        opt->requests.submitted += (i32)buf_len(result);
    }
    pop_allocator();
    buf_clear(opt->focus);
    return result;
}
