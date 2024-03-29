static f32 H = 0.0001f;
static f32 StepSize = 0.0016f;
static f32 MDecay = 0.9f;
static f32 VDecay = 0.999f;
static f32 VVDecay = 0.99f;
static i32 SampleBatchSize = 16;
static i32 ParameterBatchSize = 16;
static f32 NegativeEpsilon = 0.0f;
static f32 Regularisation = 0.01f;
static f32 RegularisationAlpha = 0.15f;
static f32 LossFunction = 0.0f;

enum {
    Param_None = -1
};

typedef struct {
    i32 param;
    f32 order;
} SworExp;

int sworexp_sort(void const *av, void const *bv)
{
    SworExp const *a = av;
    SworExp const *b = bv;
    return (a->order < b->order) ? -1 : 1;
}

// https://timvieira.github.io/blog/post/2019/09/16/algorithms-for-sampling-without-replacement/
void random_sequence_weighted(i32 arr[], f32 weights[])
{
    assert(buf_len(arr) == buf_len(weights));
    push_allocator(scratch);
    SworExp *g = 0;
    buf_pushn(g, buf_len(arr));
    for (i32 i = 0; i < buf_len(g); i++) {
        g[i].param = i;
        g[i].order = -logf(rngf()) / weights[i];
    }
    qsort(g, buf_len(g), sizeof(SworExp), sworexp_sort);
    for (i32 i = 0; i < buf_len(arr); i++) {
        arr[i] = g[i].param;
    }
    pop_allocator();
}

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

    // AMSGrad state vectors
    f32 *v;
    f32 *vv;
    f32 *m;

    // My own special juice
    f32 *w;

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
    f32 x;
    f32 y_wanted;
    f32 y_got;
    f32 weight;
} OptimizationEvaluation;

f32 loss(f32 want, f32 got, f32 weight)
{
    f32 delta = want - got;
    f32 squared_loss = delta*delta;
    f32 quantile_loss = (delta >= 0.0f) ? 2.0f * delta : -delta;
    return weight * lerp(squared_loss, quantile_loss, LossFunction);
}

void optimize(OptimizationContext *opt, i32 n_params, f32 *initial_x, i32 n_samples)
{
    opt->n_params = n_params;
    opt->n_samples = n_samples;
    opt->loss = 0.0f;
    buf_zeros(opt->x, n_params);
    buf_zeros(opt->v, n_params);
    buf_zeros(opt->vv, n_params);
    buf_zeros(opt->m, n_params);
    buf_zeros(opt->w, n_params);
    buf_zeros(opt->active.samples, n_samples);
    buf_zeros(opt->active.parameters, n_params);
    for (isize i = 0; i < n_params; i++) {
        opt->x[i] = initial_x[i];
        opt->w[i] = 1.0f / (f32)n_params;
        opt->active.parameters[i] = (i32)i;
    }
    for (isize i = 0; i < n_samples; i++) {
        opt->active.samples[i] = (i32)i;
    }
    buf_reserve(opt->focus, 8);
    buf_clear(opt->focus);
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
        f32 hsq = H*H;
        f32 w_sum = 0.0f;
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
                    loss_x += loss(x->y_wanted, x->y_got, x->weight);
                    loss_xh += loss(xh->y_wanted, xh->y_got, xh->weight);
                    regularisation_l1 += clamp(-H, H, 2 * xh->x - H);
                    regularisation_l2 += h2 * xh->x;

                    n_losses += 1.0f;
                }

                f32 penalty = Regularisation * lerp(regularisation_l1, regularisation_l2, RegularisationAlpha);

                f32 g = (penalty + (loss_xh - loss_x)) / H;
                // AMSGrad. VVDecay is not in the paper but is an obvious extension. vv stops
                // division by small v from blowing up the step size when it wobbles between
                // small and large values. But otherwise we want it to be as small as possible
                // when v is stable.
                opt->m[i] = lerp(g, opt->m[i], MDecay);
                opt->v[i] = lerp(g*g, opt->v[i], VDecay);
                opt->vv[i] = max(opt->vv[i] * VVDecay, opt->v[i]);
                f32 m = opt->m[i];
                f32 v = sqrtf(opt->vv[i] + 1e-8f);
                opt->x[i] = opt->x[i] - StepSize * m / v;
                opt->w[i] = lerp(opt->w[i] * expf(-1.0f / v),
                                 1.0f / (f32)opt->n_params,
                                 1e-5f);

                opt->loss += loss_x + penalty;
            } else {
                opt->m[i] = lerp(0, opt->m[i], MDecay);
                opt->v[i] = lerp(0, opt->v[i], VDecay);
                opt->vv[i] = max(opt->vv[i] * VVDecay, opt->v[i]);
                f32 m = opt->m[i];
                f32 v = opt->vv[i];
                opt->x[i] = opt->x[i] - StepSize * m / (sqrtf(v) + 1e-8f);
            }

            w_sum += opt->w[i];
        }

        for (isize i = 0; i < opt->n_params; i++) {
            opt->w[i] /= w_sum;
        }

        opt->loss *= (f32)(opt->n_samples * opt->n_params) / n_losses;
    }

    opt->iter++;

    random_sequence(opt->active.samples, opt->focus);
    random_sequence_weighted(opt->active.parameters, opt->w);

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
