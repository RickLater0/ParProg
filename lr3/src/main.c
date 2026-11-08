#include "common.h"
#include "parallel.h"
#include "sequential.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#define HELP_MESSAGE "Usage: %s <M> <N>\n\
            where\n\
            M, N - integer interval bounds (inclusive), M <= N\n\
            If args are omitted, 'M N' is read from stdin.\n"

#define RESULT_BUDGET_BYTES (5u * 1024u * 1024u)
#define SEQ_RESULT_FILE     "seq_result.txt"
#define PAR_RESULT_FILE     "par_result.txt"
#define TAIL_LINES          10

static lab_err_t parse_u64(
    const char *s,
    u64 *out
);

static double timespec_diff(
    const struct timespec *s,
    const struct timespec *e
);

static void print_solution(
    const char *label,
    int ranks,
    const result_t *r,
    const struct timespec *start,
    const struct timespec *end
);

static int results_equal(
    const result_t *a,
    const result_t *b
);

static void print_diff(
    const result_t *seq,
    const result_t *par
);

static size_t estimate_line_bytes(
    const triple_entry_t *e
);

static void write_result_file(
    const char *path,
    const result_t *r,
    u64 tail_start
);

static void write_both_files(
    const result_t *seq,
    const result_t *par
);

static void print_time(
    const char *label,
    const struct timespec *s,
    const struct timespec *e
);

static void handler(int sig);

int main(int argc, char **argv)
{
    u64 m = 0;
    u64 n = 0;
    int rank = 0;
    int size = 1;
    int parse_ok = 0;
    lab_err_t ret = NO_ERR;
    result_t seq_result = {0};
    result_t par_result = {0};

    ret = parallel_init(&argc, &argv);
    if (ret != NO_ERR)
    {
        printf("Error: %s\n", lab_err_string(ret));
        return 1;
    }

    rank = parallel_rank();
    size = parallel_size();

    if (rank == 0)
    {
        if (argc == 3)
        {
            if (parse_u64(argv[1], &m) == NO_ERR &&
                parse_u64(argv[2], &n) == NO_ERR)
            {
                parse_ok = 1;
            }
        }
        else if (argc == 1)
        {
            unsigned long long mm = 0;
            unsigned long long nn = 0;
            if (scanf("%llu %llu", &mm, &nn) == 2)
            {
                m = (u64)mm;
                n = (u64)nn;
                parse_ok = 1;
            }
        }
        if (!parse_ok)
        {
            printf(HELP_MESSAGE, argv[0]);
        }
        else if (m > n)
        {
            printf("Error: M > N\n");
            parse_ok = 0;
        }
    }

    ret = parallel_broadcast_input(&m, &n, &parse_ok);
    if (ret != NO_ERR || !parse_ok)
    {
        parallel_finalize();
        return 0;
    }

    signal(SIGINT,  handler);
    signal(SIGTERM, handler);

    if (rank == 0)
    {
        ret = s_solution(m, n, &seq_result);
    }
    ret = parallel_sync_err(ret);
    if (ret != NO_ERR)
    {
        if (rank == 0)
        {
            printf("Error: %s\n", lab_err_string(ret));
        }
        goto end;
    }
    if (rank == 0)
    {
        print_solution("[seq]", 0, &seq_result, &seq_start, &seq_end);
    }

    ret = p_solution(m, n, &par_result);
    if (ret != NO_ERR)
    {
        if (rank == 0)
        {
            printf("Error: %s\n", lab_err_string(ret));
        }
        goto end;
    }

    if (rank == 0)
    {
        print_solution("[par]", size, &par_result, &par_start, &par_end);
        print_diff(&seq_result, &par_result);
        write_both_files(&seq_result, &par_result);
        printf("[file] full results in: %s, %s\n",
               SEQ_RESULT_FILE, PAR_RESULT_FILE);
    }

end:
    result_free(&seq_result);
    result_free(&par_result);
    parallel_finalize();
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

static double timespec_diff(
    const struct timespec *s,
    const struct timespec *e
)
{
    if (!s || !e)
    {
        return 0.0;
    }
    if ((s->tv_sec == 0 && s->tv_nsec == 0) ||
        (e->tv_sec == 0 && e->tv_nsec == 0))
    {
        return 0.0;
    }
    return (double)(e->tv_sec - s->tv_sec)
         + (double)(e->tv_nsec - s->tv_nsec) / 1e9;
}

static void print_solution(
    const char *label,
    int ranks,
    const result_t *r,
    const struct timespec *start,
    const struct timespec *end
)
{
    u64 from = 0;
    u64 shown = 0;
    double elapsed = 0.0;

    from    = (r->count > TAIL_LINES) ? (r->count - TAIL_LINES) : 0;
    shown   = r->count - from;
    elapsed = timespec_diff(start, end);

    if (ranks > 0)
    {
        printf("%s (%d ranks): %llu triples, %.6f s\n",
               label, ranks,
               (unsigned long long)r->count, elapsed);
    }
    else
    {
        printf("%s: %llu triples, %.6f s\n",
               label,
               (unsigned long long)r->count, elapsed);
    }

    if (shown > 0)
    {
        if (shown < r->count)
        {
            printf("last %llu:\n", (unsigned long long)shown);
        }
        else
        {
            printf("triples:\n");
        }
        for (u64 i = from; i < r->count; i++)
        {
            const triple_entry_t *e = &r->entries[i];
            printf("  %llu = %llu * %llu * %llu\n",
                   (unsigned long long)e->value,
                   (unsigned long long)e->p1,
                   (unsigned long long)e->p2,
                   (unsigned long long)e->p3);
        }
    }
    printf("\n");
}

static int results_equal(
    const result_t *a,
    const result_t *b
)
{
    if (a->count != b->count)
    {
        return 0;
    }
    for (u64 i = 0; i < a->count; i++)
    {
        const triple_entry_t *ea = &a->entries[i];
        const triple_entry_t *eb = &b->entries[i];
        if (ea->value != eb->value ||
            ea->p1 != eb->p1 ||
            ea->p2 != eb->p2 ||
            ea->p3 != eb->p3)
        {
            return 0;
        }
    }
    return 1;
}

static void print_diff(
    const result_t *seq,
    const result_t *par
)
{
    printf("[diff]\n");
    printf("  seq.count = %llu, par.count = %llu\n",
           (unsigned long long)seq->count,
           (unsigned long long)par->count);

    if (results_equal(seq, par))
    {
        printf("  results: OK\n");
    }
    else
    {
        u64 lim = (seq->count < par->count) ? seq->count : par->count;
        printf("  results: MISMATCH\n");
        for (u64 i = 0; i < lim; i++)
        {
            const triple_entry_t *ea = &seq->entries[i];
            const triple_entry_t *eb = &par->entries[i];
            if (ea->value != eb->value ||
                ea->p1 != eb->p1 ||
                ea->p2 != eb->p2 ||
                ea->p3 != eb->p3)
            {
                printf("    first mismatch @ %llu:\n"
                       "      seq: %llu = %llu * %llu * %llu\n"
                       "      par: %llu = %llu * %llu * %llu\n",
                       (unsigned long long)i,
                       (unsigned long long)ea->value,
                       (unsigned long long)ea->p1,
                       (unsigned long long)ea->p2,
                       (unsigned long long)ea->p3,
                       (unsigned long long)eb->value,
                       (unsigned long long)eb->p1,
                       (unsigned long long)eb->p2,
                       (unsigned long long)eb->p3);
                break;
            }
        }
    }
    printf("\n");
}

static size_t estimate_line_bytes(
    const triple_entry_t *e
)
{
    return (size_t)snprintf(
        NULL, 0,
        "%llu = %llu * %llu * %llu\n",
        (unsigned long long)e->value,
        (unsigned long long)e->p1,
        (unsigned long long)e->p2,
        (unsigned long long)e->p3
    );
}

static void write_result_file(
    const char *path,
    const result_t *r,
    u64 tail_start
)
{
    FILE *f = fopen(path, "w");

    if (!f)
    {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return;
    }
    for (u64 i = tail_start; i < r->count; i++)
    {
        const triple_entry_t *e = &r->entries[i];
        fprintf(f, "%llu = %llu * %llu * %llu\n",
            (unsigned long long)e->value,
            (unsigned long long)e->p1,
            (unsigned long long)e->p2,
            (unsigned long long)e->p3);
    }
    fclose(f);
}

static void write_both_files(
    const result_t *seq,
    const result_t *par
)
{
    const size_t per_file = RESULT_BUDGET_BYTES / 2;
    u64 min_count = (seq->count < par->count) ? seq->count : par->count;
    size_t seq_acc = 0;
    size_t par_acc = 0;
    u64 k = 0;

    while (k < min_count)
    {
        u64 is = seq->count - 1 - k;
        u64 ip = par->count - 1 - k;
        size_t bs = estimate_line_bytes(&seq->entries[is]);
        size_t bp = estimate_line_bytes(&par->entries[ip]);

        if (seq_acc + bs > per_file)
        {
            break;
        }
        if (par_acc + bp > per_file)
        {
            break;
        }

        seq_acc += bs;
        par_acc += bp;
        k++;
    }

    write_result_file(SEQ_RESULT_FILE, seq, seq->count - k);
    write_result_file(PAR_RESULT_FILE, par, par->count - k);
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
    if ((s->tv_sec == 0 && s->tv_nsec == 0) ||
        (e->tv_sec == 0 && e->tv_nsec == 0))
    {
        return;
    }

    elapsed = timespec_diff(s, e);
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