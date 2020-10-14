static f32 H = 0.0001f;
static f32 StepSize = 0.001f;
static f32 MDecay = 0.9f;
static f32 VDecay = 0.999f;
static i32 SampleBatchSize = 16;
static i32 ParameterBatchSize = 8;

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
    i32 n_samples;
    i32 n_parameters;
    f32 h;
} OptimizationRequest;

typedef struct {
    // Current parameter vector
    f32 *x;

    // ADAM state vectors
    f32 *v;
    f32 *v_correction;
    f32 *m;
    f32 *m_correction;

    // Estimate of the loss at x, x + h. Zeroed by opt_pump.
    struct {
        f32 at_x;
        f32 *at_xh;
    } loss;

    // Samples and parameters being trained by the last batch of requests
    struct {
        i32 *samples;
        i32 *parameters;
    } active;

    // Request counters
    struct {
        u32 submitted;
        u32 completed;
    } requests;

    // Samples to look at in the next iteration; otherwise random
    i32 *focus;

    i32 n_params;
    i32 n_samples;
    i32 iter;
} OptimizationContext;

f32 barrier(f32 x, f32 a)
{
    if (x < 1.0f) {
        return x*x;
    } else {
        return (2.0f / a) * powf(x, a) + 1.0f - (a / 2.0f);
    }
}

f32 loss(f32 delta, f32 barrier_strength)
{
    f32 err = (-delta < 1.0f) ? delta*delta : 2.0f*fabsf(delta) - 1.0f;
    f32 positive_penalty = (barrier_strength <= 0.0f) ? 0.0f : barrier(delta, barrier_strength);
    return err * positive_penalty;
}

OptimizationContext optimize(i32 n_params, f32 *initial_x, i32 n_samples)
{
    OptimizationContext result = {
        .n_params = n_params,
        .n_samples = n_samples,
    };
    buf_pushn(result.x, n_params);
    buf_pushn(result.v, n_params);
    buf_pushn(result.v_correction, n_params);
    buf_pushn(result.m, n_params);
    buf_pushn(result.m_correction, n_params);
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

void opt_discard(OptimizationContext *opt)
{
    opt->loss.at_x = 0.0f;
    for (isize i = 0; i < buf_len(opt->x); i++) {
        opt->loss.at_xh[i] = 0.0f;
    }
    opt->requests.submitted = 0;
    opt->requests.completed = 0;
    opt->iter = opt->iter > 0 ? opt->iter - 1 : 0;
}

void opt_push_evaluation(OptimizationContext *opt, i32 param, f32 sample_delta, f32 weight, f32 barrier_weight)
{
    assert(sample_delta == sample_delta);
    if (param == Param_None) {
        opt->loss.at_x += weight * loss(sample_delta, barrier_weight);
    } else {
        assert(param >= 0 && param < opt->n_params);
        opt->loss.at_xh[param] += weight * loss(sample_delta, barrier_weight);
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

OptimizationRequest opt_pump(OptimizationContext *opt)
{
    OptimizationRequest result = {0};
    if (opt->requests.submitted == opt->requests.completed) {
        if (opt->requests.submitted > 0) {
            for (isize i = 0; i < buf_len(opt->x); i++) {
                // ADAM: A Method For Stochastic Optimization (2015)
                f32 g = (opt->loss.at_xh[i] == 0.0f) ? 0.0f : (opt->loss.at_xh[i] - opt->loss.at_x) / H;
                opt->m[i] = lerp(g, opt->m[i], MDecay);
                opt->v[i] = lerp(g*g, opt->v[i], VDecay);
                opt->m_correction[i] += 1.0f;
                opt->v_correction[i] += 1.0f;
                f32 m = opt->m[i] / (1.0f - powf(MDecay, opt->m_correction[i]));
                f32 v = opt->v[i] / (1.0f - powf(VDecay, opt->v_correction[i]));
                opt->x[i] = opt->x[i] - StepSize * m / (sqrtf(v) + 1e-8f);
                opt->loss.at_xh[i] = 0.0f;
            }
        }

        opt->loss.at_x = 0.0f;
        opt->iter++;

        shuffle(opt->active.samples, opt->focus);
        shuffle(opt->active.parameters, 0);

        i32 sample_batch_size = (i32)clamps(0, opt->n_samples, SampleBatchSize);
        i32 parameter_batch_size = (i32)clamps(0, opt->n_params, ParameterBatchSize);
        result = (OptimizationRequest) {
            .n_samples = sample_batch_size,
            .n_parameters = parameter_batch_size,
            .h = H
        };

        opt->requests.submitted += sample_batch_size + sample_batch_size * parameter_batch_size;
    }
    buf_clear(opt->focus);
    return result;
}
