#include "common.h"
#include "parallel.h"
#include "sequential.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include <string.h>

#define MATRIX_PRINT_LIMIT 16

#define HELP_MESSAGE "Usage: %s <n> <m1.mtrx> <m2.mtrx> \n\
            where\n\
            n - matrix size\n\
            m*.mtrx - text file with n*n whitespace-separated numbers\n\
                      (0 - nolink, >0 - linked)\n"

static lab_err_t parse_matrix_from_file(
    u8 n,
    const char *path,
    u8 ***matrix_out
);

static lab_err_t test(
    solution sol,
    const graph_t *g1,
    const graph_t *g2,
    graph_t *ret_g1,
    graph_t *ret_g2,
    u8 *res
);

static void print_out(
    const char *label,
    const graph_t *g1,
    const graph_t *g2,
    const u8 res,
    const struct timespec *start,
    const struct timespec *end
);

static void print_diff(
    const graph_t *ret_graph11,
    const graph_t *ret_graph12,
    const graph_t *ret_graph21,
    const graph_t *ret_graph22,
    u8 res_seq,
    u8 res_par
);

static void free_matrix(u8 n, u8 **m);
static void print_matrix_impl(const char *label, const graph_t *g, const char *path);
static lab_err_t write_matrix_to_file(const graph_t *g, const char *path);
static int  graphs_equal(const graph_t *a, const graph_t *b);
static void print_time(
    const char *label,
    const struct timespec *s,
    const struct timespec *e
);

static void handler(int sig);

int main(int argc, char **argv)
{
    u8 n = 0;
    u8 **matrix1 = NULL;
    u8 **matrix2 = NULL;
    u8 res_seq = 0;
    u8 res_par = 0;

    graph_t graph1 = {0};
    graph_t graph2 = {0};

    graph_t seq_ret_graph1 = {0};
    graph_t seq_ret_graph2 = {0};

    graph_t par_ret_graph1 = {0};
    graph_t par_ret_graph2 = {0};

    lab_err_t ret = NO_ERR;

    if(argc == 1)
    {
        printf(HELP_MESSAGE, argv[0]);
        return 0;
    }
    n = (u8)strtoul(argv[1], NULL, 10);
    if(argc != 4 || n == 0)
    {
        printf(HELP_MESSAGE, argv[0]);
        return 0;
    }

    if((ret = parse_matrix_from_file(
        n,
        argv[2],
        &matrix1
    )) != NO_ERR)
    {
        goto end;
    }

    if((ret = parse_matrix_from_file(
        n,
        argv[3],
        &matrix2
    )) != NO_ERR)
    {
        goto end;
    }

    if((ret = graph_init(
        &graph1,
        n,
        matrix1
    ))  != NO_ERR)
    {
        goto end;
    }

    if((ret = graph_init(
        &graph2,
        n,
        matrix2
    )) != NO_ERR)
    {
        goto end;
    }

    signal(SIGINT,  handler);
    signal(SIGTERM, handler);

    //testing sequential solution
    ret = test(
        s_solution,
        &graph1,
        &graph2,
        &seq_ret_graph1,
        &seq_ret_graph2,
        &res_seq
    );
    if(ret != NO_ERR)
    {
        goto end;
    }
    print_out(
        "[seq]",
        &seq_ret_graph1,
        &seq_ret_graph2,
        res_seq,
        &seq_start,
        &seq_end
    );

    //testing parallel solution
    ret = test(
        p_solution,
        &graph1,
        &graph2,
        &par_ret_graph1,
        &par_ret_graph2,
        &res_par
    );
    if(ret != NO_ERR)
    {
        goto end;
    }
    print_out(
        "[par]",
        &par_ret_graph1,
        &par_ret_graph2,
        res_par,
        &par_start,
        &par_end
    );

    print_diff(
        &seq_ret_graph1,
        &seq_ret_graph2,
        &par_ret_graph1,
        &par_ret_graph2,
        res_seq,
        res_par
    );

    end:
        if(ret != NO_ERR)
        {
            printf("Error occur: %s", lab_err_string(ret));
        }
        graph_free(&graph1);
        graph_free(&graph2);

        graph_free(&seq_ret_graph1);
        graph_free(&seq_ret_graph2);

        graph_free(&par_ret_graph1);
        graph_free(&par_ret_graph2);

        free_matrix(n, matrix1);
        free_matrix(n, matrix2);

        return ret == NO_ERR ? 0 : 1;
}

static lab_err_t parse_matrix_from_file(
    u8 n,
    const char *path,
    u8 ***matrix_out
)
{
    lab_err_t ret = NO_ERR;
    u8 **m = NULL;
    FILE *f = NULL;
    u32 count = 0;
    int v = 0;

    if(!n || !path || !matrix_out)
    {
        return ARG_ERR;
    }

    *matrix_out = NULL;

    f = fopen(path, "r");
    if(!f)
    {
        return ARG_ERR;
    }

    m = calloc(n, sizeof(u8*));
    if(!m)
    {
        ret = MEM_ERR;
        goto end;
    }

    for(u8 i = 0; i < n; i++)
    {
        m[i] = calloc(n, sizeof(u8));
        if(!m[i])
        {
            ret = MEM_ERR;
            goto end;
        }
    }

    while(fscanf(f, "%d", &v) == 1)
    {
        if(v < 0 || count >= (u32)n * n)
        {
            ret = ARG_ERR;
            goto end;
        }

        m[count / n][count % n] = (v > 0) ? 1 : 0;
        count++;
    }

    if(count != (u32)n * n)
    {
        ret = ARG_ERR;
        goto end;
    }

    *matrix_out = m;
    m = NULL;

    end:
        if(f)
        {
            fclose(f);
        }
        if(m)
        {
            free_matrix(n, m);
        }
        return ret;
}

static void free_matrix(u8 n, u8 **m)
{
    if(!m)
    {
        return;
    }

    for(u8 i = 0; i < n; i++)
    {
        free(m[i]);
    }
    free(m);
}

static lab_err_t write_matrix_to_file(const graph_t *g, const char *path)
{
    FILE *f = NULL;

    if(!g || !g->matrix || !path)
    {
        return ARG_ERR;
    }

    f = fopen(path, "w");
    if(!f)
    {
        return ARG_ERR;
    }

    for(u8 i = 0; i < g->n; i++)
    {
        for(u8 j = 0; j < g->n; j++)
        {
            fprintf(f, "%u%s", g->matrix[i][j], (j + 1 < g->n) ? " " : "\n");
        }
    }

    fclose(f);
    return NO_ERR;
}

static void print_matrix_impl(const char *label, const graph_t *g, const char *path)
{
    if(!g)
    {
        printf("%s: (null)\n", label);
        return;
    }

    if(g->n > MATRIX_PRINT_LIMIT)
    {
        if(path && write_matrix_to_file(g, path) == NO_ERR)
        {
            printf("%s: (%ux%u, written to %s)\n",
                   label, g->n, g->n, path);
        }
        else
        {
            printf("%s: (%ux%u, output skipped: n > %d)\n",
                   label, g->n, g->n, MATRIX_PRINT_LIMIT);
        }
        return;
    }

    printf("%s (%ux%u):\n", label, g->n, g->n);
    if(!g->matrix)
    {
        printf("  (empty)\n");
        return;
    }

    for(u8 i = 0; i < g->n; i++)
    {
        printf("  ");
        for(u8 j = 0; j < g->n; j++)
        {
            printf("%u%s", g->matrix[i][j], (j + 1 < g->n) ? " " : "");
        }
        printf("\n");
    }
}

static int graphs_equal(const graph_t *a, const graph_t *b)
{
    if(!a || !b)
    {
        return a == b;
    }
    if(a->n != b->n)
    {
        return 0;
    }
    if(!a->matrix || !b->matrix)
    {
        return a->matrix == b->matrix;
    }

    for(u8 i = 0; i < a->n; i++)
    {
        if(!a->matrix[i] || !b->matrix[i])
        {
            if(a->matrix[i] != b->matrix[i])
            {
                return 0;
            }
            continue;
        }
        for(u8 j = 0; j < a->n; j++)
        {
            if(a->matrix[i][j] != b->matrix[i][j])
            {
                return 0;
            }
        }
    }
    return 1;
}

static void print_time(
    const char *label,
    const struct timespec *s,
    const struct timespec *e
)
{
    double elapsed = 0.0;

    if(!label || !s || !e)
    {
        return;
    }

    elapsed = (e->tv_sec - s->tv_sec)
            + (e->tv_nsec - s->tv_nsec) / 1e9;

    printf("%s time: %.6f s\n", label, elapsed);
}

static lab_err_t test(
    solution sol,
    const graph_t *g1,
    const graph_t *g2,
    graph_t *ret_g1,
    graph_t *ret_g2,
    u8 *res
)
{
    if(!sol || !g1 || !g2 || !ret_g1 || !ret_g2 || !res)
    {
        return ARG_ERR;
    }

    return sol(g1, g2, ret_g1, ret_g2, res);
}

static void print_out(
    const char *label,
    const graph_t *g1,
    const graph_t *g2,
    const u8 res,
    const struct timespec *start,
    const struct timespec *end
)
{
    const char *path1 = NULL;
    const char *path2 = NULL;

    if(label && strcmp(label, "[seq]") == 0)
    {
        path1 = "m1_seq_out.mtrx";
        path2 = "m2_seq_out.mtrx";
    }
    else if(label && strcmp(label, "[par]") == 0)
    {
        path1 = "m1_par_out.mtrx";
        path2 = "m2_par_out.mtrx";
    }

    print_matrix_impl("graph1", g1, path1);
    print_matrix_impl("graph2", g2, path2);

    //предполагаю res - булев: 1 = изоморфны, 0 = нет
    printf("Isomorphic: %s (res = %u)\n",
           res ? "YES" : "NO", res);

    print_time(label, start, end);
}

static void print_diff(
    const graph_t *s1,
    const graph_t *s2,
    const graph_t *p1,
    const graph_t *p2,
    u8 res_seq,
    u8 res_par
)
{
    int direct  = graphs_equal(s1, p1) && graphs_equal(s2, p2);
    int swapped = graphs_equal(s1, p2) && graphs_equal(s2, p1);

    printf("\n[diff] sequential vs parallel:\n");

    if(direct && !swapped)
    {
        printf("  graphs: OK (s.g1 == p.g1, s.g2 == p.g2)\n");
    }
    else if(!direct && swapped)
    {
        printf("  graphs: OK (s.g1 == p.g2, s.g2 == p.g1, swapped)\n");
    }
    else if(direct && swapped)
    {
        printf("  graphs: OK (symmetric / identical)\n");
    }
    else
    {
        printf("  graphs: MISMATCH\n");
        printf("    s.g1 == p.g1 : %s\n", graphs_equal(s1, p1) ? "yes" : "no");
        printf("    s.g1 == p.g2 : %s\n", graphs_equal(s1, p2) ? "yes" : "no");
        printf("    s.g2 == p.g1 : %s\n", graphs_equal(s2, p1) ? "yes" : "no");
        printf("    s.g2 == p.g2 : %s\n", graphs_equal(s2, p2) ? "yes" : "no");

        if(!graphs_equal(s1, p1) && !graphs_equal(s1, p2))
        {
            print_matrix_impl("    s.g1", s1, NULL);
            print_matrix_impl("    p.g1", p1, NULL);
            print_matrix_impl("    p.g2", p2, NULL);
        }
        if(!graphs_equal(s2, p1) && !graphs_equal(s2, p2))
        {
            print_matrix_impl("    s.g2", s2, NULL);
            print_matrix_impl("    p.g1", p1, NULL);
            print_matrix_impl("    p.g2", p2, NULL);
        }
    }

    if(res_seq == res_par)
    {
        printf("  res: matches (%u)\n", res_seq);
    }
    else
    {
        printf("  res: MISMATCH (seq=%u, par=%u)\n", res_seq, res_par);
    }

    print_time("[seq]", &seq_start, &seq_end);
    print_time("[par]", &par_start, &par_end);
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