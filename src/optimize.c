static f32 H = 0.0001f;
static f32 StepSize = 0.0016f;
static f32 MDecay = 0.9f;
static f32 VDecay = 0.999f;
static i32 SampleBatchSize = 16;
static i32 ParameterBatchSize = 16;
static f32 NegativeEpsilon = 0.0f;
static f32 Regularisation = 0.01f;
static f32 RegularisationAlpha = 0.15f;

enum {
    Param_None = -1
};

void swap(i32 *a, i32 *b)
{
    i32 temp = *a;
    *a = *b;
    *b = temp;
}

void random_sequence(i32 arr[], i32 head[])
{
    for (i32 i = 0; i < buf_len(arr); i++) {
        arr[i] = i;
    }
    for (i32 i = 0; i < buf_len(head); i++) {
        swap(arr + i, arr + head[i]);
    }
    isize len = buf_len(arr);
    for (isize i = buf_len(head); i < len; i++) {
        isize idx = i + rngu(len - i);
        swap(arr + i, arr + idx);
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

    // Estimate of the loss at x
    f32 loss;

    // Samples and parameters being trained by the last batch of requests
    struct {
        i32 *samples;
        i32 *parameters;
    } active;

    // Samples to look at in the next iteration; otherwise random
    i32 *focus;

    i32 n_params;
    i32 n_samples;
    i32 iter;
} OptimizationContext;

typedef struct {
    i32 sample;
    i32 param;
    f32 value_difference_from_initial;
    f32 delta;
    f32 barrier;
} OptimizationEvaluation;

f32 loss(f32 delta, f32 barrier)
{
    // The complicated loss function is from before regularisation was added
    // which was more effective keeping the optimizer away from 'obviously' bad
    // solutions than these. So, disabled them.
    //
    // NegativeEpsilon adds a flat region to the base of the loss curve, on the
    // negative side of the origin. I think this is not a good idea, it will let
    // losses cluster at the edges rather than push them as high in the flat
    // region as possible. Also, it's not obvious what scale it is on.
    //
    // Barrier, for postiive x, switches from x^2 to x^barrier loss at the point
    // their derivatives are equal. So, as far as I can tell, this should be
    // well behaved for the optimizer. But it's hard to reason about so yeeting
    // it.
#if 0
    f32 x = delta;
    f32 a = barrier;
    if (x < -NegativeEpsilon) {
        return square(x + NegativeEpsilon);
    } else if (x < 0.0f) {
        return 0.0f;
    } else if (x < 1.0f) {
        return x*x;
    } else {
        return (2.0f / a) * (powf(x, a) - 1.0f) + 1.0f;
    }
#else
    return delta*delta;
#endif
}

void optimize(OptimizationContext *opt, i32 n_params, f32 *initial_x, i32 n_samples)
{
    opt->n_params = n_params;
    opt->n_samples = n_samples;
    opt->loss = 0.0f;
    buf_zeros(opt->x, n_params);
    buf_zeros(opt->v, n_params);
    buf_zeros(opt->v_correction, n_params);
    buf_zeros(opt->m, n_params);
    buf_zeros(opt->m_correction, n_params);
    buf_zeros(opt->active.samples, n_samples);
    buf_zeros(opt->active.parameters, n_params);
    for (isize i = 0; i < n_params; i++) {
        opt->x[i] = initial_x[i];
        opt->active.parameters[i] = (i32)i;
        opt->v_correction[i] = VDecay;
        opt->m_correction[i] = MDecay;
    }
    for (isize i = 0; i < n_samples; i++) {
        opt->active.samples[i] = (i32)i;
    }
    buf_reserve(opt->focus, 8);
    buf_clear(opt->focus);
    opt->iter = 0;
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

OptimizationRequest opt_pump(OptimizationContext *opt, OptimizationEvaluation evals[])
{
    OptimizationRequest result = {0};
    f32 n_losses = 0.0f;
    if (buf_len(evals) > 0) {
        opt->loss = 0.0f;

        push_allocator(scratch);
        struct { i32 *samples; } *param_evals = 0;
        i32 *sample_evals = 0;
        b8 *sample_seen = 0;
        buf_pushn(sample_evals, opt->n_samples);
        buf_pushn(sample_seen, opt->n_samples);
        buf_pushn(param_evals, opt->n_params);
        for (i32 i = 0; i < buf_len(evals); i++) {
            if (evals[i].param == Param_None) {
                sample_evals[evals[i].sample] = i;
                assert(sample_seen[evals[i].sample] == false);
                sample_seen[evals[i].sample] = true;
            } else {
                buf_push(param_evals[evals[i].param].samples, i);
            }
        }
        pop_allocator();

        f32 h2 = 2 * H;
        f32 hsq = square(H);
        for (isize i = 0; i < opt->n_params; i++) {
            if (param_evals[i].samples) {
                f32 regularisation_l1 = 0;
                f32 regularisation_l2 = -hsq * (f32)buf_len(param_evals[i].samples);
                f32 loss_x = 0.0f;
                f32 loss_xh = 0.0f;
                for (isize j = 0; j < buf_len(param_evals[i].samples); j++) {
                    OptimizationEvaluation *xh = &evals[param_evals[i].samples[j]];
                    assert(sample_seen[xh->sample]);
                    OptimizationEvaluation *x = &evals[sample_evals[xh->sample]];
                    loss_x += loss(x->delta, x->barrier);
                    loss_xh += loss(xh->delta, xh->barrier);
                    regularisation_l1 += clamp(-H, H, 2 * xh->value_difference_from_initial - H);
                    regularisation_l2 += h2 * xh->value_difference_from_initial;
                }

                f32 penalty = Regularisation * lerp(regularisation_l1, regularisation_l2, RegularisationAlpha);

                f32 g = (penalty + loss_xh - loss_x) / H;
                // ADAM: A Method For Stochastic Optimization (2015)
                opt->m[i] = lerp(g, opt->m[i], MDecay);
                opt->v[i] = lerp(g*g, opt->v[i], VDecay);
                f32 m = opt->m[i] / (1.0f - opt->m_correction[i]);
                f32 v = opt->v[i] / (1.0f - opt->v_correction[i]);
                opt->m_correction[i] *= MDecay;
                opt->v_correction[i] *= VDecay;
                opt->x[i] = opt->x[i] - StepSize * m / (sqrtf(v) + 1e-8f);

                opt->loss += loss_x + penalty;
                n_losses += 1.0f;
            } else {
                opt->m[i] = lerp(0, opt->m[i], MDecay);
                opt->v[i] = lerp(0, opt->v[i], VDecay);
                f32 m = opt->m[i] / (1.0f - opt->m_correction[i]);
                f32 v = opt->v[i] / (1.0f - opt->v_correction[i]);
                opt->x[i] = opt->x[i] - StepSize * m / (sqrtf(v) + 1e-8f);
            }
        }
    }

    opt->iter++;

    opt->loss /= n_losses;

    random_sequence(opt->active.samples, opt->focus);
    random_sequence(opt->active.parameters, 0);

    i32 sample_batch_size = (i32)clamps(0, opt->n_samples, SampleBatchSize);
    i32 parameter_batch_size = (i32)clamps(0, opt->n_params, ParameterBatchSize);
    result = (OptimizationRequest) {
        .n_samples = sample_batch_size,
        .n_parameters = parameter_batch_size,
        .h = H
    };
    buf_clear(opt->focus);
    return result;
}
