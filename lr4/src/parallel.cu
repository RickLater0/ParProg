#include "parallel.h"

#include <cuda_runtime.h>
#include <string.h>

#define BLOCK_X 8
#define BLOCK_Y 8
#define BLOCK_Z 8

__global__ void jacobi_step_kernel(
    const f64 *__restrict__ old_grid,
    f64 *__restrict__ new_grid,
    const u8 *__restrict__ mask,
    u64 nx,
    u64 ny,
    u64 nz
)
{
    u64 x = (u64)blockIdx.x * blockDim.x + threadIdx.x;
    u64 y = (u64)blockIdx.y * blockDim.y + threadIdx.y;
    u64 z = (u64)blockIdx.z * blockDim.z + threadIdx.z;
    u64 stride_y = nx;
    u64 stride_z = nx * ny;
    u64 idx = 0;
    u8  mt = 0;
    f64 sum = 0.0;
    f64 new_val = 0.0;
    u32 cnt = 0;

    if (x >= nx || y >= ny || z >= nz)
    {
        return;
    }

    idx = x + y * stride_y + z * stride_z;
    mt  = mask[idx];

    if (mt != 1)
    {
        new_grid[idx] = old_grid[idx];
        return;
    }

    if (x > 0 && mask[idx - 1] != 0)
    {
        sum += old_grid[idx - 1];
        cnt++;
    }
    if (x + 1 < nx && mask[idx + 1] != 0)
    {
        sum += old_grid[idx + 1];
        cnt++;
    }
    if (y > 0 && mask[idx - stride_y] != 0)
    {
        sum += old_grid[idx - stride_y];
        cnt++;
    }
    if (y + 1 < ny && mask[idx + stride_y] != 0)
    {
        sum += old_grid[idx + stride_y];
        cnt++;
    }
    if (z > 0 && mask[idx - stride_z] != 0)
    {
        sum += old_grid[idx - stride_z];
        cnt++;
    }
    if (z + 1 < nz && mask[idx + stride_z] != 0)
    {
        sum += old_grid[idx + stride_z];
        cnt++;
    }

    new_val = (cnt > 0) ? (sum / (f64)cnt) : old_grid[idx];
    new_grid[idx] = new_val;
}

solution p_solution = parallel;
struct timespec par_start = {0};
struct timespec par_end   = {0};

/**
 * @brief Параллельное решение задачи методом Якоби на CUDA GPU.
 * @param params   Параметры задачи.
 * @param top_face Выходной массив k*m с температурой верхней грани.
 * @return NO_ERR при успехе, иначе код ошибки.
 */
lab_err_t parallel(
    const params_t *params,
    f64 *top_face
)
{
    lab_err_t err = NO_ERR;
    int device_count = 0;
    cudaError_t cuda_ret;
    cudaDeviceProp prop = {0};
    grid_t host_grid = {0};
    u8 *host_mask = NULL;
    u8 *dev_mask  = NULL;
    f64 *dev_a = NULL;
    f64 *dev_b = NULL;
    f64 *cur_dev = NULL;
    f64 *nxt_dev = NULL;
    f64 *tmp_dev = NULL;
    u64 total = 0;
    u64 top_size = 0;
    u64 top_plane = 0;
    u64 iter = 0;
    dim3 block_dim(BLOCK_X, BLOCK_Y, BLOCK_Z);
    dim3 grid_dim;

    if (!params || !top_face)
    {
        return ARG_ERR;
    }

    cuda_ret = cudaGetDeviceCount(&device_count);
    if (cuda_ret != cudaSuccess || device_count == 0)
    {
        return CUDA_NO_DEVICE;
    }

    cuda_ret = cudaGetDeviceProperties(&prop, 0);
    if (cuda_ret != cudaSuccess)
    {
        return CUDA_ERR;
    }

    total     = params->k * params->m * params->n;
    top_size  = params->k * params->m;
    top_plane = (params->n - 1) * params->k * params->m;

    err = grid_init(&host_grid, params->k, params->m, params->n);
    if (err != NO_ERR)
    {
        return err;
    }

    host_mask = (u8 *)malloc((size_t)total * sizeof(u8));
    if (!host_mask)
    {
        grid_free(&host_grid);
        return MEM_ERR;
    }

    err = grid_setup(&host_grid, host_mask, params);
    if (err != NO_ERR)
    {
        grid_free(&host_grid);
        free(host_mask);
        return err;
    }

    cuda_ret = cudaMalloc((void **)&dev_a, (size_t)total * sizeof(f64));
    if (cuda_ret != cudaSuccess)
    {
        grid_free(&host_grid);
        free(host_mask);
        return CUDA_ERR;
    }

    cuda_ret = cudaMalloc((void **)&dev_b, (size_t)total * sizeof(f64));
    if (cuda_ret != cudaSuccess)
    {
        cudaFree(dev_a);
        grid_free(&host_grid);
        free(host_mask);
        return CUDA_ERR;
    }

    cuda_ret = cudaMalloc((void **)&dev_mask, (size_t)total * sizeof(u8));
    if (cuda_ret != cudaSuccess)
    {
        cudaFree(dev_a);
        cudaFree(dev_b);
        grid_free(&host_grid);
        free(host_mask);
        return CUDA_ERR;
    }

    cuda_ret = cudaMemcpy(dev_a, host_grid.data,
                          (size_t)total * sizeof(f64),
                          cudaMemcpyHostToDevice);
    if (cuda_ret != cudaSuccess)
    {
        err = CUDA_ERR;
        goto cleanup;
    }

    cuda_ret = cudaMemcpy(dev_b, host_grid.data,
                          (size_t)total * sizeof(f64),
                          cudaMemcpyHostToDevice);
    if (cuda_ret != cudaSuccess)
    {
        err = CUDA_ERR;
        goto cleanup;
    }

    cuda_ret = cudaMemcpy(dev_mask, host_mask,
                          (size_t)total * sizeof(u8),
                          cudaMemcpyHostToDevice);
    if (cuda_ret != cudaSuccess)
    {
        err = CUDA_ERR;
        goto cleanup;
    }

    grid_dim.x = (unsigned)((params->k + BLOCK_X - 1) / BLOCK_X);
    grid_dim.y = (unsigned)((params->m + BLOCK_Y - 1) / BLOCK_Y);
    grid_dim.z = (unsigned)((params->n + BLOCK_Z - 1) / BLOCK_Z);

    jacobi_step_kernel<<<grid_dim, block_dim>>>(
        dev_a, dev_b, dev_mask,
        params->k, params->m, params->n);
    cudaDeviceSynchronize();

    cur_dev = dev_a;
    nxt_dev = dev_b;

    clock_gettime(CLOCK_MONOTONIC, &par_start);

    for (iter = 0; iter < params->max_iter; iter++)
    {
        jacobi_step_kernel<<<grid_dim, block_dim>>>(
            cur_dev, nxt_dev, dev_mask,
            params->k, params->m, params->n);

        tmp_dev = cur_dev;
        cur_dev = nxt_dev;
        nxt_dev = tmp_dev;
    }

    cudaDeviceSynchronize();
    clock_gettime(CLOCK_MONOTONIC, &par_end);

    cuda_ret = cudaMemcpy(top_face, cur_dev + top_plane,
                          (size_t)top_size * sizeof(f64),
                          cudaMemcpyDeviceToHost);
    if (cuda_ret != cudaSuccess)
    {
        err = CUDA_ERR;
    }

cleanup:
    cudaFree(dev_a);
    cudaFree(dev_b);
    cudaFree(dev_mask);
    grid_free(&host_grid);
    free(host_mask);
    return err;
}