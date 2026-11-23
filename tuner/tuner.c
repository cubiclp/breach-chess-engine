// tuner.c - Texel tuner using gradient descent, with evaluate() treated as a black box.
//
// There is no eval trace and no second copy of the evaluation. Instead:
//
//   1. PROBE.  For every position, call evaluate() once, then once more per weight with
//      that weight bumped by STEP. The change in the score is that weight's coefficient
//      (how many times it is counted in this position). This is done once, for the whole
//      dataset, and only the non-zero coefficients are kept (~80 per position).
//
//   2. TRAIN.  Adam on the cached model  score = base + sum(coef * (w - w_start)).
//      No evaluate() calls, so an epoch is just a pass over a few hundred MB of memory.
//
//   3. VERIFY. Round the weights, write them, and re-run the real evaluate() over the
//      dataset. If that loss matches the model's loss, the linear model was right.
//
// This works because the eval is linear in (almost) all weights. The exceptions are the
// king-danger parameters, which are frozen (see freeze_nonlinear).
// The same tuner keeps working when you add or change eval terms, as long as a new weight
// enters the score linearly. If it does not, the VERIFY step shows a mismatch.
//
// Usage: initialise the engine as usual (attack tables, init_eval(), ...), then call
//        tune("weights.txt", "positions.txt", 1000000);
// The tuner overwrites the board position, so set up your own position again afterwards.

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/board.h"
#include "../src/eval.h"
#include "tuner.h"
#include "../src/weights.h"

static void set_position(char *fen) { parse_fen(fen); }   // must set bitboards, occupancies, side

// ---- settings ------------------------------------------------------------------------

#define STEP        960     // probe size in weight units; a multiple of 24 so the phase taper divides exactly
#define EPOCHS      1000
#define LEARN_RATE  1.0     // Adam step size in weight units; decays linearly to zero
#define HOLDOUT     20      // every 20th position is only used for validation

// ---- weights as one flat vector ------------------------------------------------------

#define N_WEIGHTS  ((int) (sizeof(weights) / sizeof(I16)))   // weights per phase
#define N_PARAMS   (2 * N_WEIGHTS)                           // parameter k: mg_weights[k], then eg_weights[k - N_WEIGHTS]

static I16 *param(int k)
{
    return k < N_WEIGHTS ? (I16 *) &mg_weights + k : (I16 *) &eg_weights + (k - N_WEIGHTS);
}

static char frozen[N_PARAMS];   // parameters that are not tuned

static void freeze(size_t byte_offset, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        int k = (int) (byte_offset / sizeof(I16) + i);
        frozen[k] = frozen[N_WEIGHTS + k] = 1;
    }
}
#define FREEZE(field) freeze(offsetof(weights, field), sizeof(((weights *) 0)->field) / sizeof(I16))

// Weights that do not enter the score linearly, so the probe cannot model them.
static void freeze_nonlinear(void)
{
    memset(frozen, 0, sizeof frozen);
    FREEZE(king_attack_weight);     // squared inside the king danger term
    FREEZE(king_danger_div);
    FREEZE(king_danger_max);
}

// ---- dataset: sparse linear model of evaluate() around the starting weights ------------

typedef struct {
    uint16_t idx;    // parameter index
    int16_t  c24;    // d(score)/d(weight) * 24, always an integer (phase is in 24ths)
} Term;

static int     n_pos;
static float  *base;      // evaluate() at the starting weights, white's point of view
static float  *result;    // 1 = white won, 0.5 = draw, 0 = black won
static size_t *start;     // terms of position i are terms[start[i] .. start[i + 1])
static Term   *terms;
static size_t  n_terms, cap_terms;

static int is_val(int i) { return i % HOLDOUT == 0; }

static int eval_white(void)   // evaluate() from white's point of view
{
    int s = evaluate();
    return side == white ? s : -s;
}

static void free_dataset(void)
{
    free(base);   base = NULL;
    free(result); result = NULL;
    free(start);  start = NULL;
    free(terms);  terms = NULL;
    n_pos = 0;
    n_terms = cap_terms = 0;
}

static int add_term(int k, int c24)
{
    if (n_terms == cap_terms)
    {
        Term *bigger = realloc(terms, cap_terms * 2 * sizeof(Term));
        if (!bigger) return 0;
        terms = bigger;
        cap_terms *= 2;
    }
    terms[n_terms].idx = (uint16_t) k;
    terms[n_terms].c24 = (int16_t) c24;
    n_terms++;
    return 1;
}

// Record position n_pos (already set on the board).
//
// Bumping an mg weight by STEP changes the final score by coef * STEP * phase / 24, and an eg
// weight by coef * STEP * (24 - phase) / 24. STEP is a multiple of 24, so that is an exact
// integer, apart from +-1 from the integer truncation in evaluate(). Dividing by STEP / 24
// and rounding therefore recovers coef * phase (or coef * (24 - phase)) exactly.
static int probe_position(void)
{
    int e0 = eval_white();
    base[n_pos] = (float) e0;
    start[n_pos] = n_terms;

    for (int k = 0; k < N_PARAMS; k++)
    {
        if (frozen[k]) continue;

        I16 *w = param(k);
        I16 saved = *w;
        *w = (I16) (saved + STEP);
        int delta = eval_white() - e0;
        *w = saved;

        int c24 = (int) lround(24.0 * delta / STEP);
        if (c24 != 0 && !add_term(k, c24)) return 0;
    }
    return 1;
}

// Cuts line down to the FEN and returns the result (1, 0.5 or 0), or -1 if the line has none.
// Understands  <fen> "1-0"   <fen> c9 "1-0";   <fen> 1-0   (also 0-1 and 1/2-1/2),  and  <fen> [1.0].
static double parse_line(char *line)
{
    char *cut = strchr(line, '[');
    if (cut)
    {
        double r = atof(cut + 1);
        *cut = '\0';
        return (r >= 0.0 && r <= 1.0) ? r : -1.0;
    }

    static const struct { const char *text; double result; } marks[] = {
        { "1/2-1/2", 0.5 }, { "1-0", 1.0 }, { "0-1", 0.0 }
    };

    double r = -1.0;
    for (int i = 0; i < 3; i++)
    {
        char *at = strstr(line, marks[i].text);
        if (at && (!cut || at < cut)) { cut = at; r = marks[i].result; }
    }
    if (!cut) return -1.0;

    if (cut > line && cut[-1] == '"') cut--;            // opening quote
    char *c9 = strstr(line, " c9");
    if (c9 && c9 < cut) cut = c9;                       // Zurichess-style "c9" tag
    *cut = '\0';
    return r;
}

static int load_dataset(const char *path, int max_pos)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror("positions file"); return 0; }

    int lines = 0;
    for (int c; (c = fgetc(f)) != EOF; ) lines += c == '\n';
    rewind(f);
    if (max_pos > 0 && lines > max_pos) lines = max_pos;

    base   = malloc(lines * sizeof(float));
    result = malloc(lines * sizeof(float));
    start  = malloc((lines + 1) * sizeof(size_t));
    cap_terms = (size_t) lines * 64 + 1024;
    terms  = malloc(cap_terms * sizeof(Term));
    if (!base || !result || !start || !terms)
    {
        fprintf(stderr, "out of memory\n");
        fclose(f);
        return 0;
    }

    n_pos = 0;
    n_terms = 0;

    char line[1024];
    time_t t0 = time(NULL);

    while (n_pos < lines && fgets(line, sizeof line, f))
    {
        double r = parse_line(line);
        if (r < 0.0) continue;

        set_position(line);
        result[n_pos] = (float) r;
        if (!probe_position()) { fprintf(stderr, "out of memory\n"); fclose(f); return 0; }
        n_pos++;

        if (n_pos % 100000 == 0)
        {
            double elapsed = difftime(time(NULL), t0);
            printf("  probed %d / %d positions, %.0fs elapsed, ~%.0fs left\n",
                   n_pos, lines, elapsed, elapsed * (lines - n_pos) / n_pos);
            fflush(stdout);
        }
    }
    fclose(f);

    start[n_pos] = n_terms;
    printf("probed %d positions, %.1f non-zero coefficients each, %.0f MB, %.0fs\n",
           n_pos, n_pos ? (double) n_terms / n_pos : 0.0,
           (n_pos * 12.0 + n_terms * sizeof(Term)) / 1e6, difftime(time(NULL), t0));

    if (n_pos < 2 * HOLDOUT)
    {
        fprintf(stderr, "only %d usable positions in %s (need a result like \"1-0\" at the end of each line)\n", n_pos, path);
        return 0;
    }
    return 1;
}

// ---- loss and gradient (mean squared error through a sigmoid) --------------------------

static inline double model_score(int i, const double *theta)   // theta = change from the starting weights
{
    if (!theta) return base[i];

    double dot = 0;
    for (size_t t = start[i]; t < start[i + 1]; t++)
        dot += terms[t].c24 * theta[terms[t].idx];
    return base[i] + dot / 24.0;
}

static inline double sigmoid(double score, double K)
{
    return 1.0 / (1.0 + exp(-K * score));
}

// Mean squared error on the training (validation = 0) or validation (1) split.
static double mse(const double *theta, double K, int validation)
{
    double sum = 0;
    long count = 0;

    for (int i = 0; i < n_pos; i++)
    {
        if (is_val(i) != validation) continue;
        double err = sigmoid(model_score(i, theta), K) - result[i];
        sum += err * err;
        count++;
    }
    return sum / count;
}

// Training loss, and its gradient with respect to theta in grad[N_PARAMS].
static double mse_grad(const double *theta, double K, double *grad)
{
    double sum = 0;
    long count = 0;
    memset(grad, 0, N_PARAMS * sizeof(double));

    for (int i = 0; i < n_pos; i++)
    {
        if (is_val(i)) continue;

        double p = sigmoid(model_score(i, theta), K);
        double err = p - result[i];
        sum += err * err;
        count++;

        double d = 2.0 * err * p * (1.0 - p) * K / 24.0;   // d(loss) / d(sum of c24 * theta)
        for (size_t t = start[i]; t < start[i + 1]; t++)
            grad[terms[t].idx] += d * terms[t].c24;
    }

    for (int k = 0; k < N_PARAMS; k++) grad[k] /= count;
    return sum / count;
}

// The sigmoid scale K (per centipawn) that fits the starting weights best.
static double fit_k(void)
{
    double lo = 0.0005, hi = 0.05;
    for (int it = 0; it < 40; it++)
    {
        double a = lo + (hi - lo) / 3, b = hi - (hi - lo) / 3;
        if (mse(NULL, a, 0) < mse(NULL, b, 0)) hi = b; else lo = a;
    }
    return (lo + hi) / 2;
}

// ---- Adam ------------------------------------------------------------------------------

static void train(double *theta, double K)
{
    double grad[N_PARAMS], m[N_PARAMS] = {0}, v[N_PARAMS] = {0};
    const double b1 = 0.9, b2 = 0.999, eps = 1e-8;
    time_t t0 = time(NULL);

    for (int e = 1; e <= EPOCHS; e++)
    {
        double loss = mse_grad(theta, K, grad);
        double lr = LEARN_RATE * (1.0 - (double) (e - 1) / EPOCHS);
        double c1 = 1.0 - pow(b1, e), c2 = 1.0 - pow(b2, e);

        for (int k = 0; k < N_PARAMS; k++)
        {
            m[k] = b1 * m[k] + (1.0 - b1) * grad[k];
            v[k] = b2 * v[k] + (1.0 - b2) * grad[k] * grad[k];
            theta[k] -= lr * (m[k] / c1) / (sqrt(v[k] / c2) + eps);
        }

        if (e == 1 || e % 100 == 0)
        {
            printf("  epoch %4d  lr %.3f  train %.6f  val %.6f  (%.0fs)\n",
                   e, lr, loss, mse(theta, K, 1), difftime(time(NULL), t0));
            fflush(stdout);
        }
    }
}

// ---- verification with the real evaluate() ---------------------------------------------

static void real_mse(const char *path, double K, double *train_mse, double *val_mse)
{
    double sum[2] = {0, 0};
    long count[2] = {0, 0};

    FILE *f = fopen(path, "r");
    if (f)
    {
        char line[1024];
        for (int i = 0; i < n_pos && fgets(line, sizeof line, f); )
        {
            double r = parse_line(line);
            if (r < 0.0) continue;

            set_position(line);
            double err = sigmoid(eval_white(), K) - r;
            sum[is_val(i)] += err * err;
            count[is_val(i)]++;
            i++;
        }
        fclose(f);
    }

    *train_mse = sum[0] / count[0];
    *val_mse   = sum[1] / count[1];
}

// ---- entry point -----------------------------------------------------------------------

static int run(char *weights_file, char *positions_file, int max_positions)
{
    FILE *check = fopen(weights_file, "r");
    if (!check) { perror("weights file"); return -1; }
    fclose(check);

    // parse_weights_from_file() only reports problems on stderr, so detect a failed parse here:
    // a successful one overwrites the sentinel. Evaluating with all-zero weights would divide by zero.
    weights old_mg = mg_weights, old_eg = eg_weights;
    memset(&mg_weights, 0x7F, sizeof mg_weights);
    deinit_weights();                           // forget the template of an earlier load
    parse_weights_from_file(weights_file);
    if (mg_weights.material[0] == 0x7F7F)
    {
        mg_weights = old_mg;
        eg_weights = old_eg;
        fprintf(stderr, "tune: could not read %d weights from %s, nothing was changed\n", 2 * N_WEIGHTS, weights_file);
        return -1;
    }
    freeze_nonlinear();

    double w_start[N_PARAMS], theta[N_PARAMS] = {0};
    for (int k = 0; k < N_PARAMS; k++)
    {
        w_start[k] = *param(k);
        if (!frozen[k] && abs(*param(k)) + STEP > INT16_MAX)
        {
            fprintf(stderr, "weight %d is too large to probe with STEP = %d\n", k, STEP);
            return -1;
        }
    }

    if (!load_dataset(positions_file, max_positions)) return -1;

    double K = fit_k();
    printf("K = %.5f per cp (Texel K = %.3f)\n", K, K * 400.0 / log(10.0));
    printf("start:  train %.6f  val %.6f\n", mse(theta, K, 0), mse(theta, K, 1));

    train(theta, K);

    // round to the integers the engine uses, and write them back through the engine's own writer
    int changed = 0;
    for (int k = 0; k < N_PARAMS; k++)
    {
        long w = lround(w_start[k] + theta[k]);
        w = w < INT16_MIN ? INT16_MIN : w > INT16_MAX ? INT16_MAX : w;
        theta[k] = (double) w - w_start[k];
        *param(k) = (I16) w;
        changed += w != (long) w_start[k];
    }
    write_weights(weights_file);

    double real_train, real_val;
    real_mse(positions_file, K, &real_train, &real_val);
    printf("tuned:  train %.6f  val %.6f   (model, rounded weights)\n", mse(theta, K, 0), mse(theta, K, 1));
    printf("tuned:  train %.6f  val %.6f   (real evaluate())\n", real_train, real_val);
    printf("%d weights changed, written to %s\n", changed, weights_file);
    return 0;
}

int tune(char *weights_file, char *positions_file, int max_positions)
{
    eval_set_pawn_cache(0);       // cached pawn scores would be stale after every weight change
    int status = run(weights_file, positions_file, max_positions);
    eval_set_pawn_cache(1);       // also flushes entries computed with the weights we probed
    free_dataset();
    return status;
}