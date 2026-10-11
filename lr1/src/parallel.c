#define CL_TARGET_OPENCL_VERSION 300

#include "common.h"
#include "parallel.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define BATCH_SIZE 1024

static const char *kernel_source = 
"\n\
#define u64 unsigned long\n\
#define u8 unsigned char\n\
\n\
__kernel void check_candidates(\n\
    __global const u64 *primes,\n\
    const int p_sz,\n\
    const int k,\n\
    __global const u64 *candidates,\n\
    const int cand_count,\n\
    __global int *results,\n\
    __global u64 *out_combinations\n\
) {\n\
    int gid = get_global_id(0);\n\
    if (gid >= cand_count) return;\n\
\n\
    u64 target = candidates[gid];\n\
    int found = 0;\n\
\n\
    if (k == 1) {\n\
        int low = 0, high = p_sz - 1;\n\
        while (low <= high) {\n\
            int mid = low + (high - low) / 2;\n\
            if (primes[mid] == target) {\n\
                found = 1;\n\
                out_combinations[gid * k] = primes[mid];\n\
                break;\n\
            }\n\
            else if (primes[mid] < target) low = mid + 1;\n\
            else high = mid - 1;\n\
        }\n\
        results[gid] = found;\n\
        return;\n\
    }\n\
\n\
    int iter_stack_idx[16];\n\
    int iter_prime_idx[16];\n\
    for(int i = 0; i < 16; i++) {\n\
        iter_stack_idx[i] = 0;\n\
        iter_prime_idx[i] = 0;\n\
    }\n\
\n\
    int d = 0;\n\
    iter_prime_idx[0] = 0;\n\
    u64 c_sum = 0;\n\
\n\
    while (1) {\n\
        if (d == (int)k) {\n\
            if (c_sum == target) {\n\
                found = 1;\n\
                for (int i = 0; i < k; i++) {\n\
                    out_combinations[gid * (int)k + i] = primes[iter_stack_idx[i]];\n\
                }\n\
                break;\n\
            }\n\
            d--;\n\
            if (d < 0) break;\n\
            c_sum -= primes[iter_stack_idx[d]];\n\
            iter_prime_idx[d]++;\n\
        } else {\n\
            int start = iter_prime_idx[d];\n\
            if (p_sz - start < (int)k - d || (c_sum + primes[start] > target && primes[start] > 0)) {\n\
                d--;\n\
                if (d < 0) break;\n\
                c_sum -= primes[iter_stack_idx[d]];\n\
                iter_prime_idx[d]++;\n\
                continue;\n\
            }\n\
            if (start < p_sz) {\n\
                u64 p_val = primes[start];\n\
                if (c_sum + p_val > target) {\n\
                    d--;\n\
                    if (d < 0) break;\n\
                    c_sum -= primes[iter_stack_idx[d]];\n\
                    iter_prime_idx[d]++;\n\
                } else {\n\
                    iter_stack_idx[d] = start;\n\
                    c_sum += p_val;\n\
                    d++;\n\
                    if (d < (int)k) {\n\
                        iter_prime_idx[d] = start + 1;\n\
                    }\n\
                }\n\
            } else {\n\
                d--;\n\
                if (d < 0) break;\n\
                c_sum -= primes[iter_stack_idx[d]];\n\
                iter_prime_idx[d]++;\n\
            }\n\
        }\n\
    }\n\
    results[gid] = found;\n\
    }\n\
";

lab_err_t parallel(
    const u64 N,
    const size_t k,
    u64 **M
)
{
    u64 *primes = NULL;
    size_t p_sz = 0;
    lab_err_t ret = NO_ERR;
    u64 min_possible_sum = 0;

    cl_int err;
    cl_platform_id platform = NULL;
    cl_device_id device = NULL;
    cl_uint num_platforms, num_devices;

    cl_program program = NULL;
    cl_kernel kernel = NULL;

    cl_mem d_primes = NULL;
    cl_mem d_candidates = NULL;
    cl_mem d_results = NULL;
    cl_mem d_combinations = NULL;
    cl_context context = NULL;
    cl_command_queue queue = NULL;

    u64 candidates[BATCH_SIZE];
    int results[BATCH_SIZE];
    u64 *host_combinations = NULL;
    u64 *best_comb = NULL;
    u64 current_target = N - 1;

    if (!M)
    {
        return ARG_ERR;
    }

    if ((ret = sieve(N, &primes, &p_sz)) != NO_ERR)
    {
        return ret;
    }

    if (k > p_sz)
    {
        free(primes);
        return LARGE_K;
    }

    for (size_t i = 0; i < k; i++)
    {
        min_possible_sum += primes[i];
    }

    if (N <= min_possible_sum)
    {
        free(primes);
        return NO_SUM;
    }

    if (clGetPlatformIDs(
            1, 
            &platform, 
            &num_platforms
        ) != CL_SUCCESS || 
        num_platforms == 0
    )
    {
        free(primes);
        return PAR_NO_DEVICE;
    }
    
    if (clGetDeviceIDs(
            platform, 
            CL_DEVICE_TYPE_GPU, 
            1, 
            &device, 
            &num_devices
        ) != CL_SUCCESS
    )
    {
        if (clGetDeviceIDs(
                platform, 
                CL_DEVICE_TYPE_CPU, 
                1, 
                &device, 
                &num_devices
            ) != CL_SUCCESS
        )
        {
            free(primes);
            return PAR_NO_DEVICE;
        }
    }

    context = clCreateContext(
        NULL, 
        1, 
        &device, 
        NULL, 
        NULL, 
        &err
    );
    if (err != CL_SUCCESS || !context)
    {
        free(primes);
        return PAR_INIT_ERR;
    }

    queue = clCreateCommandQueueWithProperties(
        context, 
        device, 
        0, 
        &err
    );
    if (err != CL_SUCCESS || !queue)
    {
        clReleaseContext(context);
        free(primes);
        return PAR_INIT_ERR;
    }

    program = clCreateProgramWithSource(
        context, 
        1, 
        &kernel_source, 
        NULL, 
        &err
    );
    if (err != CL_SUCCESS || !program)
    {
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(primes);
        return PAR_INIT_ERR;
    }

    if (
        clBuildProgram(
            program, 
            1, 
            &device, 
            NULL, 
            NULL, 
            NULL
        ) != CL_SUCCESS
    )
    {
        size_t log_size;
        char *log = NULL;
        clGetProgramBuildInfo(
            program, 
            device, 
            CL_PROGRAM_BUILD_LOG, 
            0, 
            NULL, 
            &log_size
        );
        log = malloc(log_size);
        if (log) {
            clGetProgramBuildInfo(
                program, 
                device, 
                CL_PROGRAM_BUILD_LOG, 
                log_size, 
                log, 
                NULL
            );
            printf("OpenCL Build Error:\n%s\n", log);
            free(log);
        }
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(primes);
        return PAR_PROG_BUILD_ERR;
    }

    kernel = clCreateKernel(
        program, 
        "check_candidates", 
        &err
    );
    if (err != CL_SUCCESS || !kernel)
    {
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(primes);
        return PAR_INIT_ERR;
    }

    host_combinations = malloc(BATCH_SIZE * k * sizeof(u64));
    if (!host_combinations)
    {
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(primes);
        return MEM_ERR;
    }

    d_primes = clCreateBuffer(
        context, 
        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
        p_sz * sizeof(u64), 
        primes, 
        &err
    );
    d_candidates = clCreateBuffer(context, CL_MEM_READ_ONLY, BATCH_SIZE * sizeof(u64), NULL, &err);
    d_results = clCreateBuffer(context, CL_MEM_WRITE_ONLY, BATCH_SIZE * sizeof(int), NULL, &err);
    d_combinations = clCreateBuffer(context, CL_MEM_WRITE_ONLY, BATCH_SIZE * k * sizeof(u64), NULL, &err);

    if (!d_primes || !d_candidates || !d_results || !d_combinations)
    {
        if (d_primes) clReleaseMemObject(d_primes);
        if (d_candidates) clReleaseMemObject(d_candidates);
        if (d_results) clReleaseMemObject(d_results);
        if (d_combinations) clReleaseMemObject(d_combinations);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(host_combinations);
        free(primes);
        return PAR_MEM_ERR;
    }

    if (clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_primes) != CL_SUCCESS ||
        clSetKernelArg(kernel, 1, sizeof(int), &p_sz) != CL_SUCCESS ||
        clSetKernelArg(kernel, 2, sizeof(int), &k) != CL_SUCCESS ||
        clSetKernelArg(kernel, 3, sizeof(cl_mem), &d_candidates) != CL_SUCCESS ||
        clSetKernelArg(kernel, 5, sizeof(cl_mem), &d_results) != CL_SUCCESS ||
        clSetKernelArg(kernel, 6, sizeof(cl_mem), &d_combinations) != CL_SUCCESS)
    {
        clReleaseMemObject(d_primes);
        clReleaseMemObject(d_candidates);
        clReleaseMemObject(d_results);
        clReleaseMemObject(d_combinations);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(host_combinations);
        free(primes);
        return PAR_ARG_ERR;
    }

    while (current_target >= min_possible_sum)
    {
        int batch_count = 0;
        size_t global_work_size;

        while (batch_count < BATCH_SIZE && current_target >= min_possible_sum)
        {
            candidates[batch_count++] = current_target;
            if (current_target == 0) break;
            current_target--;
        }

        if (batch_count == 0) break;

        if (clEnqueueWriteBuffer(queue, d_candidates, CL_TRUE, 0, 
                                 batch_count * sizeof(u64), candidates, 0, NULL, NULL) != CL_SUCCESS)
        {
            ret = PAR_EXEC_ERR;
            break;
        }

        if (clSetKernelArg(kernel, 4, sizeof(int), &batch_count) != CL_SUCCESS)
        {
            ret = PAR_ARG_ERR;
            break;
        }

        global_work_size = ((batch_count + 63) / 64) * 64;
        if (global_work_size == 0) global_work_size = 64;

        err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
        if (err != CL_SUCCESS)
        {
            ret = PAR_EXEC_ERR;
            break;
        }
        clFinish(queue);

        if (err == CL_SUCCESS)
        {
            clEnqueueReadBuffer(queue, d_results, CL_TRUE, 0, 
                                batch_count * sizeof(int), results, 0, NULL, NULL);
            clEnqueueReadBuffer(queue, d_combinations, CL_TRUE, 0, 
                                batch_count * k * sizeof(u64), host_combinations, 0, NULL, NULL);

            int found_idx = -1;
            for (int i = 0; i < batch_count; i++)
            {
                if (results[i] == 1)
                {
                    found_idx = i;
                    break;
                }
            }

            if (found_idx != -1)
            {
                best_comb = malloc(k * sizeof(u64));
                if (!best_comb)
                {
                    ret = MEM_ERR;
                    break;
                }
                for (size_t j = 0; j < k; j++)
                {
                    best_comb[j] = host_combinations[found_idx * k + j];
                }
                break;
            }
        }
    }

    if (d_primes) clReleaseMemObject(d_primes);
    if (d_candidates) clReleaseMemObject(d_candidates);
    if (d_results) clReleaseMemObject(d_results);
    if (d_combinations) clReleaseMemObject(d_combinations);
    if (kernel) clReleaseKernel(kernel);
    if (program) clReleaseProgram(program);
    if (queue) clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);
    if (host_combinations) free(host_combinations);
    if (primes) free(primes);

    if (!best_comb && ret == NO_ERR)
    {
        return NO_SUM;
    }

    if (ret != NO_ERR)
    {
        if (best_comb) free(best_comb);
        return ret;
    }

    *M = best_comb;
    return NO_ERR;
}