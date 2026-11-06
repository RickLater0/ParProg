#include "common.h"

#include <stdlib.h>
#include <string.h>

static int cmp_u8(const void *a, const void *b);

lab_err_t graph_init(
    graph_t *graph,
    u8 n,
    u8 **matrix
)
{
    lab_err_t ret = NO_ERR;

    graph->n = 0;
    graph->matrix = NULL;

    if(!graph || !matrix || n == 0)
    {
        ret = ARG_ERR;
        goto end;
    }

    graph->matrix = calloc(n, sizeof(*graph->matrix));
    if(!graph->matrix)
    {
        ret = MEM_ERR;
        goto end;
    }
    graph->n = n;

    for(u8 i = 0; i < n; i++)
    {
        if (!matrix[i]) {
            ret = ARG_ERR;
            goto end;
        }
        graph->matrix[i] = calloc(n, sizeof(u8));
        if(!graph->matrix[i])
        {
            ret = MEM_ERR;
            goto end;
        }

        for(u8 j = 0; j < n; j++)
        {
            graph->matrix[i][j] = matrix[i][j];
        }
    }

    end:
        if(ret != NO_ERR)
        {
            graph_free(graph);
        }
        return ret;
}

void graph_free(graph_t *graph)
{
    if(!graph || !graph->matrix)
    {
        return;
    }

    for(u8 i = 0; i < graph->n; i++)
    {
        free(graph->matrix[i]);
    }
    free(graph->matrix);

    graph->matrix = NULL;
    graph->n = 0;
}

u32 count_ones(const graph_t *g)
{
    u32 count = 0;

    for(u8 i = 0; i < g->n; i++)
    {
        for(u8 j = 0; j < g->n; j++)
        {
            if(g->matrix[i][j])
            {
                count++;
            }
        }
    }

    return count;
}

static int cmp_u8(const void *a, const void *b)
{
    return (int)(*(const u8*)a) - (int)(*(const u8*)b);
}


int degree_equal(
    const graph_t *g1,
    const graph_t *g2
)
{
    u8 *d1 = NULL;
    u8 *d2 = NULL;
    int ok = 0;

    if(g1->n != g2->n)
    {
        return 0;
    }

    d1 = calloc(g1->n, sizeof(u8));
    d2 = calloc(g2->n, sizeof(u8));
    if(!d1 || !d2)
    {
        goto end;
    }

    for(u8 i = 0; i < g1->n; i++)
    {
        for(u8 j = 0; j < g1->n; j++)
        {
            if(g1->matrix[i][j])
            {
                d1[i]++;
            }
            if(g2->matrix[i][j])
            {
                d2[i]++;
            }
        }
    }

    qsort(d1, g1->n, sizeof(u8), cmp_u8);
    qsort(d2, g2->n, sizeof(u8), cmp_u8);

    ok = (memcmp(d1, d2, (size_t)g1->n * sizeof(u8)) == 0);

    end:
        free(d1);
        free(d2);
        return ok;
}

const char *lab_err_string(lab_err_t err)
{
    switch (err) 
    {
        case NO_ERR:
            return "No error";
        case ARG_ERR:
            return "Some args are NULL";
        case MEM_ERR:
            return "Not enough memory";
        
        case PAR_NO_DEVICE:
            return "OpenCL device not found";
        case PAR_PROG_BUILD_ERR:
            return "OpenCL program build failed";
        case PAR_INIT_ERR:
            return "OpenCL context or queue initialization failed";
        case PAR_MEM_ERR:
            return "OpenCL buffer allocation failed";
        case PAR_ARG_ERR:
            return "OpenCL kernel argument setting failed";
        case PAR_EXEC_ERR:
            return "OpenCL kernel execution failed";
        
        default:
            return MODULE_NAME " Unknown error";
    }
}