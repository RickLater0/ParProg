#include "common.h"
#include "sequential.h"

#include <stdlib.h>
#include <time.h>

static int iso_backtrack(
    const graph_t *g1,
    const graph_t *g2,
    u8 *mapping,
    u8 *used,
    u8 depth
);

static lab_err_t copy_graph(
    graph_t *dst,
    const graph_t *src
);

static lab_err_t copy_graph_permuted(
    graph_t *dst,
    const graph_t *src,
    const u8 *mapping
);

solution s_solution = sequential;
struct timespec seq_start = {0};
struct timespec seq_end = {0};

static int iso_backtrack(
    const graph_t *g1,
    const graph_t *g2,
    u8 *mapping,
    u8 *used,
    u8 depth
)
{
    u8 n = g1->n;

    if(depth == n)
    {
        return 1;
    }

    u8 i = depth;
    for(u8 j = 0; j < n; j++)
    {
        if(used[j])
        {
            continue;
        }

        int ok = 1;
        for(u8 k = 0; k < i; k++)
        {
            if(g1->matrix[i][k] != g2->matrix[j][mapping[k]])
            {
                ok = 0;
                break;
            }
        }
        if(!ok)
        {
            continue;
        }

        mapping[i] = j;
        used[j] = 1;

        if(iso_backtrack(g1, g2, mapping, used, depth + 1))
        {
            return 1;
        }

        used[j] = 0;
    }

    return 0;
}

static lab_err_t copy_graph(
    graph_t *dst,
    const graph_t *src
)
{
    if(!dst || !src || !src->matrix)
    {
        return ARG_ERR;
    }

    dst->n = 0;
    dst->matrix = NULL;

    if(src->n == 0)
    {
        return NO_ERR;
    }

    return graph_init(dst, src->n, src->matrix);
}

static lab_err_t copy_graph_permuted(
    graph_t *dst,
    const graph_t *src,
    const u8 *mapping
)
{
    lab_err_t ret = NO_ERR;
    u8 n = 0;

    if(!dst || !src || !mapping)
    {
        return ARG_ERR;
    }

    n = src->n;
    dst->n = 0;
    dst->matrix = NULL;

    if(n == 0)
    {
        return NO_ERR;
    }

    dst->matrix = calloc(n, sizeof(u8*));
    if(!dst->matrix)
    {
        ret = MEM_ERR;
        goto end;
    }
    dst->n = n;

    for(u8 i = 0; i < n; i++)
    {
        dst->matrix[i] = calloc(n, sizeof(u8));
        if(!dst->matrix[i])
        {
            ret = MEM_ERR;
            goto end;
        }

        for(u8 j = 0; j < n; j++)
        {
            dst->matrix[i][j] = src->matrix[mapping[i]][mapping[j]];
        }
    }

    end:
        if(ret != NO_ERR)
        {
            graph_free(dst);
        }
        return ret;
}

lab_err_t sequential(
    const graph_t *g1,
    const graph_t *g2,
    graph_t *ret_g1,
    graph_t *ret_g2,
    u8 *res
)
{
    lab_err_t ret = NO_ERR;
    u8 n = 0;
    u8 *mapping = NULL;
    u8 *used = NULL;
    int found = 0;
    seq_end.tv_sec = 0;
    seq_end.tv_nsec = 0;

    if(!g1 || !g2 || !ret_g1 || !ret_g2 || !res)
    {
        return ARG_ERR;
    }

    *res = 0;
    ret_g1->n = 0;
    ret_g1->matrix = NULL;
    ret_g2->n = 0;
    ret_g2->matrix = NULL;

    clock_gettime(CLOCK_MONOTONIC, &seq_start);

    if(g1->n != g2->n)
    {
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR)
        {
            goto end;
        }
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR)
        {
            goto end;
        }
        goto end;
    }

    n = g1->n;

    if(n == 0)
    {
        *res = 1;
        goto end;
    }

    if(count_ones(g1) != count_ones(g2))
    {
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR)
        {
            goto end;
        }
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR)
        {
            goto end;
        }
        goto end;
    }

    if(!degree_equal(g1, g2))
    {
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR)
        {
            goto end;
        }
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR)
        {
            goto end;
        }
        goto end;
    }

    mapping = calloc(n, sizeof(u8));
    used = calloc(n, sizeof(u8));
    if(!mapping || !used)
    {
        ret = MEM_ERR;
        goto end;
    }

    found = iso_backtrack(g1, g2, mapping, used, 0);

    if(found)
    {
        *res = 1;
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR)
        {
            goto end;
        }
        if((ret = copy_graph_permuted(ret_g2, g2, mapping)) != NO_ERR)
        {
            goto end;
        }
    }
    else
    {
        *res = 0;
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR)
        {
            goto end;
        }
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR)
        {
            goto end;
        }
    }

    end:
        free(mapping);
        free(used);
        clock_gettime(CLOCK_MONOTONIC, &seq_end);
        return ret;
}