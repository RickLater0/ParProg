#include "common.h"

#include <string.h>

static int in_cylinder(
    u64 x,
    u64 y,
    u64 k,
    u64 m,
    u64 radius
)
{
    f64 cx = (f64)(k - 1) * 0.5;
    f64 cy = (f64)(m - 1) * 0.5;
    f64 dx = (f64)x - cx;
    f64 dy = (f64)y - cy;
    f64 r  = (f64)radius;

    return (dx * dx + dy * dy) < (r * r);
}

lab_err_t grid_init(
    grid_t *g,
    u64 dim_x,
    u64 dim_y,
    u64 dim_z
)
{
    u64 total = 0;

    if (!g || dim_x == 0 || dim_y == 0 || dim_z == 0)
    {
        return ARG_ERR;
    }
    if (dim_x > SIZE_MAX / dim_y)
    {
        return ARG_ERR;
    }
    if (dim_x * dim_y > SIZE_MAX / dim_z)
    {
        return ARG_ERR;
    }

    total = dim_x * dim_y * dim_z;
    g->data = (double *)malloc((size_t)total * sizeof(f64));
    if (!g->data)
    {
        return MEM_ERR;
    }

    g->dim_x = dim_x;
    g->dim_y = dim_y;
    g->dim_z = dim_z;
    memset(g->data, 0, (size_t)total * sizeof(f64));
    return NO_ERR;
}

void grid_free(
    grid_t *g
)
{
    if (!g)
    {
        return;
    }
    free(g->data);
    g->data  = NULL;
    g->dim_x = 0;
    g->dim_y = 0;
    g->dim_z = 0;
}

lab_err_t grid_setup(
    grid_t *g,
    u8 *mask,
    const params_t *p
)
{
    u64 x = 0;
    u64 y = 0;
    u64 z = 0;
    u64 material_count = 0;

    if (!g || !mask || !p || !g->data)
    {
        return ARG_ERR;
    }

    for (z = 0; z < g->dim_z; z++)
    {
        for (y = 0; y < g->dim_y; y++)
        {
            for (x = 0; x < g->dim_x; x++)
            {
                u64 idx = x + y * g->dim_x + z * g->dim_x * g->dim_y;

                if (in_cylinder(x, y, g->dim_x, g->dim_y, p->radius))
                {
                    mask[idx] = 0;
                    g->data[idx] = 0.0;
                    continue;
                }

                material_count++;

                if (z == 0)
                {
                    mask[idx] = 2;
                    g->data[idx] = 0.0;
                }
                else if (z == g->dim_z - 1 && x == 0)
                {
                    mask[idx] = 2;
                    g->data[idx] = p->t1;
                }
                else if (z == g->dim_z - 1 && x == g->dim_x - 1)
                {
                    mask[idx] = 2;
                    g->data[idx] = p->t2;
                }
                else
                {
                    f64 fx = (g->dim_x > 1)
                           ? (f64)x / (f64)(g->dim_x - 1) : 0.0;
                    f64 fz = (g->dim_z > 1)
                           ? (f64)z / (f64)(g->dim_z - 1) : 0.0;
                    f64 t_top = p->t1 + (p->t2 - p->t1) * fx;

                    mask[idx] = 1;
                    g->data[idx] = t_top * fz;
                }
            }
        }
    }

    if (material_count == 0)
    {
        return ARG_ERR;
    }
    return NO_ERR;
}

void grid_extract_top(
    const grid_t *g,
    f64 *top_face
)
{
    u64 x = 0;
    u64 y = 0;
    u64 plane = 0;

    if (!g || !top_face || !g->data)
    {
        return;
    }

    plane = (g->dim_z - 1) * g->dim_x * g->dim_y;
    for (y = 0; y < g->dim_y; y++)
    {
        for (x = 0; x < g->dim_x; x++)
        {
            u64 src = plane + x + y * g->dim_x;
            u64 dst = x + y * g->dim_x;
            top_face[dst] = g->data[src];
        }
    }
}

const char *lab_err_string(
    lab_err_t err
)
{
    switch (err)
    {
    case NO_ERR:
        return "No error";
    case ARG_ERR:
        return "Invalid arguments";
    case MEM_ERR:
        return "Out of memory";
    case CUDA_NO_DEVICE:
        return "No CUDA device found";
    case CUDA_ERR:
        return "CUDA error";
    default:
        return MODULE_NAME " Unknown error";
    }
}