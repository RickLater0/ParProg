#include "common.h"
#include "sequential.h"
#include "parallel.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define HELP_MESSAGE "Usage: %s <k> <m> <n> <T1> <T2> <radius> <max_iter>\n\
            where\n\
            k, m, n  - grid dimensions (k*m*n parallelepiped)\n\
            T1, T2   - temperatures on opposite edges of top face\n\
            radius   - radius of cylindrical cavity (in cells)\n\
            max_iter - number of Jacobi iterations\n\
            If args are omitted, the same data is read from stdin.\n"

static lab_err_t parse_u64(
    const char *s,
    u64 *out
);

static lab_err_t parse_f64(
    const char *s,
    f64 *out
);

static void print_out(
    const char *label,
    const params_t *p,
    const f64 *top_face,
    const struct timespec *start,
    const struct timespec *end
);

static int results_equal(
    const f64 *a,
    const f64 *b,
    u64 n,
    f64 tol
);

static void print_diff(
    const f64 *seq,
    const f64 *par,
    const params_t *p
);

static void print_time(
    const char *label,
    const struct timespec *s,
    const struct timespec *e
);

static void handler(int sig);

int main(int argc, char **argv)
{
    params_t params = {0};
    f64 *seq_top = NULL;
    f64 *par_top = NULL;
    u64 top_size = 0;
    lab_err_t ret = NO_ERR;
    int parse_ok = 0;

    if (argc == 8)
    {
        parse_ok =
            (parse_u64(argv[1], &params.k)        == NO_ERR) &&
            (parse_u64(argv[2], &params.m)        == NO_ERR) &&
            (parse_u64(argv[3], &params.n)        == NO_ERR) &&
            (parse_f64(argv[4], &params.t1)       == NO_ERR) &&
            (parse_f64(argv[5], &params.t2)       == NO_ERR) &&
            (parse_u64(argv[6], &params.radius)   == NO_ERR) &&
            (parse_u64(argv[7], &params.max_iter) == NO_ERR);
    }
    else if (argc == 1)
    {
        unsigned long long kk = 0;
        unsigned long long mm = 0;
        unsigned long long nn = 0;
        unsigned long long rr = 0;
        unsigned long long it = 0;
        double t1 = 0.0;
        double t2 = 0.0;

        if (scanf("%llu %llu %llu %lf %lf %llu %llu",
                  &kk, &mm, &nn, &t1, &t2, &rr, &it) == 7)
        {
            params.k = (u64)kk;
            params.m = (u64)mm;
            params.n = (u64)nn;
            params.t1 = (f64)t1;
            params.t2 = (f64)t2;
            params.radius = (u64)rr;
            params.max_iter = (u64)it;
            parse_ok = 1;
        }
    }

    if (!parse_ok)
    {
        printf(HELP_MESSAGE, argv[0]);
        return 1;
    }
    if (params.k < 3 || params.m < 3 || params.n < 3)
    {
        printf("Error: grid dimensions must be >= 3\n");
        return 1;
    }
    if (params.max_iter == 0)
    {
        printf("Error: max_iter must be > 0\n");
        return 1;
    }

    top_size = params.k * params.m;
    seq_top = malloc((size_t)top_size * sizeof(f64));
    par_top = malloc((size_t)top_size * sizeof(f64));
    if (!seq_top || !par_top)
    {
        printf("Error occur: %s\n", lab_err_string(MEM_ERR));
        free(seq_top);
        free(par_top);
        return 1;
    }

    signal(SIGINT,  handler);
    signal(SIGTERM, handler);

    printf("Parameters:\n");
    printf("  grid      : %llu x %llu x %llu\n",
           (unsigned long long)params.k,
           (unsigned long long)params.m,
           (unsigned long long)params.n);
    printf("  T1, T2    : %.6f, %.6f\n", params.t1, params.t2);
    printf("  radius    : %llu\n", (unsigned long long)params.radius);
    printf("  max_iter  : %llu\n", (unsigned long long)params.max_iter);
    printf("\n");

    ret = s_solution(&params, seq_top);
    if (ret != NO_ERR)
    {
        printf("Error occur: %s\n", lab_err_string(ret));
        goto end;
    }
    print_out("[seq]", &params, seq_top, &seq_start, &seq_end);

    ret = p_solution(&params, par_top);
    if (ret != NO_ERR)
    {
        printf("Error occur: %s\n", lab_err_string(ret));
        goto end;
    }
    print_out("[par]", &params, par_top, &par_start, &par_end);

    print_diff(seq_top, par_top, &params);

end:
    free(seq_top);
    free(par_top);
    return ret == NO_ERR ? 0 : 1;
}

static lab_err_t parse_u64(
    const char *s,
    u64 *out
)
{
    char *end = NULL;
    unsigned long long v = 0;

    if (!s || !out || !*s)
    {
        return ARG_ERR;
    }

    errno = 0;
    v = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0')
    {
        return ARG_ERR;
    }

    *out = (u64)v;
    return NO_ERR;
}

static lab_err_t parse_f64(
    const char *s,
    f64 *out
)
{
    char *end = NULL;
    double v = 0.0;

    if (!s || !out || !*s)
    {
        return ARG_ERR;
    }

    errno = 0;
    v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0')
    {
        return ARG_ERR;
    }

    *out = (f64)v;
    return NO_ERR;
}

static void print_out(
    const char *label,
    const params_t *p,
    const f64 *top_face,
    const struct timespec *start,
    const struct timespec *end
)
{
    u64 i = 0;
    f64 t_min = top_face[0];
    f64 t_max = top_face[0];
    f64 sum = 0.0;

    printf("=== %s ===\n", label);
    for (i = 0; i < p->k * p->m; i++)
    {
        if (top_face[i] < t_min)
        {
            t_min = top_face[i];
        }
        if (top_face[i] > t_max)
        {
            t_max = top_face[i];
        }
        sum += top_face[i];
    }
    printf("  top face : %llu x %llu\n",
           (unsigned long long)p->k,
           (unsigned long long)p->m);
    printf("  Tmin     : %.6f\n", t_min);
    printf("  Tmax     : %.6f\n", t_max);
    printf("  Tmean    : %.6f\n", sum / (f64)(p->k * p->m));
    print_time(label, start, end);
}

static int results_equal(
    const f64 *a,
    const f64 *b,
    u64 n,
    f64 tol
)
{
    u64 i = 0;

    for (i = 0; i < n; i++)
    {
        if (fabs(a[i] - b[i]) > tol)
        {
            return 0;
        }
    }
    return 1;
}

static void print_diff(
    const f64 *seq,
    const f64 *par,
    const params_t *p
)
{
    u64 i = 0;
    u64 total = p->k * p->m;
    f64 max_diff = 0.0;
    f64 tol = 1e-6;

    for (i = 0; i < total; i++)
    {
        f64 d = fabs(seq[i] - par[i]);
        if (d > max_diff)
        {
            max_diff = d;
        }
    }

    printf("\n[diff] sequential vs parallel:\n");
    printf("  cells         : %llu\n", (unsigned long long)total);
    printf("  max |seq-par| : %.6e\n", max_diff);
    printf("  results       : %s\n",
           results_equal(seq, par, total, tol) ? "OK" : "MISMATCH");

    print_time("[seq]", &seq_start, &seq_end);
    print_time("[par]", &par_start, &par_end);

    if (par_start.tv_sec != 0 || par_start.tv_nsec != 0)
    {
        if (par_end.tv_sec != 0 || par_end.tv_nsec != 0)
        {
            f64 ts = (seq_end.tv_sec - seq_start.tv_sec)
                   + (seq_end.tv_nsec - seq_start.tv_nsec) / 1e9;
            f64 tp = (par_end.tv_sec - par_start.tv_sec)
                   + (par_end.tv_nsec - par_start.tv_nsec) / 1e9;

            if (tp > 0.0)
            {
                printf("[cmp] speedup: %.2fx\n", ts / tp);
            }
        }
    }
}

static void print_time(
    const char *label,
    const struct timespec *s,
    const struct timespec *e
)
{
    double elapsed = 0.0;

    if (!label || !s || !e)
    {
        return;
    }
    if (s->tv_sec == 0 && s->tv_nsec == 0)
    {
        return;
    }
    if (e->tv_sec == 0 && e->tv_nsec == 0)
    {
        return;
    }

    elapsed = (e->tv_sec - s->tv_sec)
            + (e->tv_nsec - s->tv_nsec) / 1e9;
    printf("%s time: %.6f s\n", label, elapsed);
}

static void handler(int sig)
{
    struct timespec now = {0};

    clock_gettime(CLOCK_MONOTONIC, &now);

    if (seq_start.tv_sec != 0 || seq_start.tv_nsec != 0)
    {
        if (seq_end.tv_sec == 0 && seq_end.tv_nsec == 0)
        {
            print_time("[seq] (interrupted)", &seq_start, &now);
        }
    }
    if (par_start.tv_sec != 0 || par_start.tv_nsec != 0)
    {
        if (par_end.tv_sec == 0 && par_end.tv_nsec == 0)
        {
            print_time("[par] (interrupted)", &par_start, &now);
        }
    }

    signal(sig, SIG_DFL);
    raise(sig);
}