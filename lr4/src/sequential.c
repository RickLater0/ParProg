#include "sequential.h"

#include <string.h>

solution s_solution = sequential;
struct timespec seq_start = {0};
struct timespec seq_end   = {0};

/**
 * @brief Последовательное решение задачи методом Якоби на CPU.
 * @param params   Параметры задачи.
 * @param top_face Выходной массив k*m с температурой верхней грани.
 * @return NO_ERR при успехе, иначе код ошибки.
 */
lab_err_t sequential(
    const params_t *params,
    f64 *top_face
)
{
    lab_err_t err = NO_ERR;
    grid_t g_a = {0};
    grid_t g_b = {0};
    u8 *mask = NULL;
    f64 *cur = NULL;
    f64 *nxt = NULL;
    f64 *tmp = NULL;
    u64 total = 0;
    u64 stride_y = 0;
    u64 stride_z = 0;
    u64 iter = 0;
    u64 z = 0;
    u64 y = 0;
    u64 x = 0;

    if (!params || !top_face)
    {
        return ARG_ERR;
    }

    total    = params->k * params->m * params->n;
    stride_y = params->k;
    stride_z = params->k * params->m;

    mask = malloc((size_t)total * sizeof(u8));
    if (!mask)
    {
        return MEM_ERR;
    }

    err = grid_init(&g_a, params->k, params->m, params->n);
    if (err != NO_ERR)
    {
        free(mask);
        return err;
    }

    err = grid_init(&g_b, params->k, params->m, params->n);
    if (err != NO_ERR)
    {
        grid_free(&g_a);
        free(mask);
        return err;
    }

    err = grid_setup(&g_a, mask, params);
    if (err != NO_ERR)
    {
        grid_free(&g_a);
        grid_free(&g_b);
        free(mask);
        return err;
    }

    memcpy(g_b.data, g_a.data, (size_t)total * sizeof(f64));

    cur = g_a.data;
    nxt = g_b.data;

    clock_gettime(CLOCK_MONOTONIC, &seq_start);

    for (iter = 0; iter < params->max_iter; iter++)
    {
        for (z = 0; z < params->n; z++)
        {
            for (y = 0; y < params->m; y++)
            {
                for (x = 0; x < params->k; x++)
                {
                    u64 idx = x + y * stride_y + z * stride_z;
                    u8  mt  = mask[idx];
                    f64 sum = 0.0;
                    f64 new_val = 0.0;
                    u32 cnt = 0;

                    if (mt != 1)
                    {
                        nxt[idx] = cur[idx];
                        continue;
                    }

                    if (x > 0 && mask[idx - 1] != 0)
                    {
                        sum += cur[idx - 1];
                        cnt++;
                    }
                    if (x + 1 < params->k && mask[idx + 1] != 0)
                    {
                        sum += cur[idx + 1];
                        cnt++;
                    }
                    if (y > 0 && mask[idx - stride_y] != 0)
                    {
                        sum += cur[idx - stride_y];
                        cnt++;
                    }
                    if (y + 1 < params->m && mask[idx + stride_y] != 0)
                    {
                        sum += cur[idx + stride_y];
                        cnt++;
                    }
                    if (z > 0 && mask[idx - stride_z] != 0)
                    {
                        sum += cur[idx - stride_z];
                        cnt++;
                    }
                    if (z + 1 < params->n && mask[idx + stride_z] != 0)
                    {
                        sum += cur[idx + stride_z];
                        cnt++;
                    }

                    new_val = (cnt > 0) ? (sum / (f64)cnt) : cur[idx];
                    nxt[idx] = new_val;
                }
            }
        }

        tmp = cur;
        cur = nxt;
        nxt = tmp;
    }

    clock_gettime(CLOCK_MONOTONIC, &seq_end);

    {
        u64 top_plane = (params->n - 1) * stride_z;

        for (y = 0; y < params->m; y++)
        {
            for (x = 0; x < params->k; x++)
            {
                top_face[x + y * params->k] = cur[top_plane + x + y * stride_y];
            }
        }
    }

    grid_free(&g_a);
    grid_free(&g_b);
    free(mask);
    return NO_ERR;
}