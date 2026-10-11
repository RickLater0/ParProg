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

// Оптимизированный OpenCL kernel: проверяет комбинации для каждого кандидата параллельно
static const char *kernel_source = 
"__kernel void find_max_sum(\n"
"    __global const unsigned long *primes,\n"
"    const int p_sz,\n"
"    const int k,\n"
"    __global const unsigned long *candidates,\n"
"    const int cand_count,\n"
"    __global int *results,\n"
"    __global unsigned long *out_combinations\n"
") {\n"
"    int gid = get_global_id(0);\n"
"    if (gid >= cand_count) return;\n"
"\n"
"    unsigned long target = candidates[gid];\n"
"\n"
"    // Специальный случай для k = 1 (бинарный поиск в отсортированном массиве простых чисел)\n"
"    if (k == 1) {\n"
"        int low = 0, high = p_sz - 1;\n"
"        while (low <= high) {\n"
"            int mid = low + (high - low) / 2;\n"
"            if (primes[mid] == target) {\n"
"                results[gid] = 1;\n"
"                out_combinations[gid] = primes[mid];\n"
"                return;\n"
"            } else if (primes[mid] < target) {\n"
"                low = mid + 1;\n"
"            } else {\n"
"                high = mid - 1;\n"
"            }\n"
"        }\n"
"        results[gid] = 0;\n"
"        return;\n"
"    }\n"
"\n"
"    // Общий случай для k > 1 (итеративный поиск с фиксированной глубиной стек-памяти)\n"
"    int stack_idx[16];\n"
"    int prime_idx[16];\n"
"    for(int i = 0; i < 16; i++) { stack_idx[i] = 0; prime_idx[i] = 0; }\n"
"\n"
"    int d = 0;\n"
"    prime_idx[0] = 0;\n"
"    unsigned long c_sum = 0;\n"
"    int found = 0;\n"
"\n"
"    while (1) {\n"
"        if (d == k) {\n"
"            if (c_sum == target) {\n"
"                found = 1;\n"
"                for (int i = 0; i < k; i++) {\n"
"                    out_combinations[gid * k + i] = primes[stack_idx[i]];\n"
"                }\n"
"                break;\n"
"            }\n"
"            d--;\n"
"            if (d < 0) break;\n"
"            c_sum -= primes[stack_idx[d]];\n"
"            prime_idx[d]++;\n"
"        } else {\n"
"            int start = prime_idx[d];\n"
"            if (p_sz - start < k - d || (c_sum + primes[start] > target)) {\n"
"                d--;\n"
"                if (d < 0) break;\n"
"                c_sum -= primes[stack_idx[d]];\n"
"                prime_idx[d]++;\n"
"                continue;\n"
"            }\n"
"            if (start < p_sz) {\n"
"                stack_idx[d] = start;\n"
"                c_sum += primes[start];\n"
"                d++;\n"
"                if (d < k) {\n"
"                    prime_idx[d] = start + 1;\n"
"                }\n"
"            } else {\n"
"                d--;\n"
"                if (d < 0) break;\n"
"                c_sum -= primes[stack_idx[d]];\n"
"                prime_idx[d]++;\n"
"            }\n"
"        }\n"
"    }\n"
"    results[gid] = found;\n"
"}\n";

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

    u64 *candidates = NULL;
    int *results = NULL;
    u64 *host_combinations = NULL;
    u64 *best_comb = NULL;
    u64 current_target = N - 1;

    if (!M) return ARG_ERR;

    if ((ret = sieve(N, &primes, &p_sz)) != NO_ERR) {
        return ret;
    }

    if (k > p_sz || k > 16) {
        free(primes);
        return LARGE_K;
    }

    for (size_t i = 0; i < k; i++) {
        min_possible_sum += primes[i];
    }

    if (N <= min_possible_sum) {
        free(primes);
        return NO_SUM;
    }

    // Формируем полный массив кандидатов от N-1 до min_possible_sum на хосте одним махом
    int cand_count = (int)(current_target >= min_possible_sum ? (current_target - min_possible_sum + 1) : 0);
    if (cand_count <= 0) {
        free(primes);
        return NO_SUM;
    }

    candidates = malloc(cand_count * sizeof(u64));
    results = malloc(cand_count * sizeof(int));
    host_combinations = malloc(cand_count * k * sizeof(u64));

    if (!candidates || !results || !host_combinations) {
        free(candidates); free(results); free(host_combinations); free(primes);
        return MEM_ERR;
    }

    for (int i = 0; i < cand_count; i++) {
        candidates[i] = current_target--;
    }

    // Инициализация OpenCL
    if (clGetPlatformIDs(1, &platform, &num_platforms) != CL_SUCCESS || num_platforms == 0) {
        free(candidates); free(results); free(host_combinations); free(primes);
        return PAR_NO_DEVICE;
    }
    
    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, &num_devices) != CL_SUCCESS) {
        if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, &num_devices) != CL_SUCCESS) {
            free(candidates); free(results); free(host_combinations); free(primes);
            return PAR_NO_DEVICE;
        }
    }

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS || !context) {
        free(candidates); free(results); free(host_combinations); free(primes);
        return PAR_INIT_ERR;
    }

#ifdef CL_VERSION_2_0
    queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
#else
    queue = clCreateCommandQueue(context, device, 0, &err);
#endif
    if (err != CL_SUCCESS || !queue) {
        clReleaseContext(context);
        free(candidates); free(results); free(host_combinations); free(primes);
        return PAR_INIT_ERR;
    }

    program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    if (err != CL_SUCCESS || !program) {
        clReleaseCommandQueue(queue); clReleaseContext(context);
        free(candidates); free(results); free(host_combinations); free(primes);
        return PAR_INIT_ERR;
    }

    if (clBuildProgram(program, 1, &device, NULL, NULL, NULL) != CL_SUCCESS) {
        clReleaseProgram(program); clReleaseCommandQueue(queue); clReleaseContext(context);
        free(candidates); free(results); free(host_combinations); free(primes);
        return PAR_PROG_BUILD_ERR;
    }

    kernel = clCreateKernel(program, "find_max_sum", &err);
    if (err != CL_SUCCESS || !kernel) {
        clReleaseProgram(program); clReleaseCommandQueue(queue); clReleaseContext(context);
        free(candidates); free(results); free(host_combinations); free(primes);
        return PAR_INIT_ERR;
    }

    // Создание буферов на GPU
    d_primes = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, p_sz * sizeof(u64), primes, &err);
    d_candidates = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cand_count * sizeof(u64), candidates, &err);
    d_results = clCreateBuffer(context, CL_MEM_WRITE_ONLY, cand_count * sizeof(int), NULL, &err);
    d_combinations = clCreateBuffer(context, CL_MEM_WRITE_ONLY, cand_count * k * sizeof(u64), NULL, &err);

    if (!d_primes || !d_candidates || !d_results || !d_combinations) {
        ret = PAR_MEM_ERR;
        goto cleanup;
    }

    // Установка аргументов ядра
    int int_k = (int)k;
    int int_p_sz = (int)p_sz;

    if (clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_primes) != CL_SUCCESS ||
        clSetKernelArg(kernel, 1, sizeof(int), &int_p_sz) != CL_SUCCESS ||
        clSetKernelArg(kernel, 2, sizeof(int), &int_k) != CL_SUCCESS ||
        clSetKernelArg(kernel, 3, sizeof(cl_mem), &d_candidates) != CL_SUCCESS ||
        clSetKernelArg(kernel, 4, sizeof(int), &cand_count) != CL_SUCCESS ||
        clSetKernelArg(kernel, 5, sizeof(cl_mem), &d_results) != CL_SUCCESS ||
        clSetKernelArg(kernel, 6, sizeof(cl_mem), &d_combinations) != CL_SUCCESS) {
        ret = PAR_ARG_ERR;
        goto cleanup;
    }

    // Запуск ядра на все кандидаты сразу одним вызовом
    size_t global_work_size = ((cand_count + 255) / 255) * 255;
    if (global_work_size == 0) global_work_size = 256;

    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        ret = PAR_EXEC_ERR;
        goto cleanup;
    }

    // Чтение результатов обратно на хост
    clEnqueueReadBuffer(queue, d_results, CL_TRUE, 0, cand_count * sizeof(int), results, 0, NULL, NULL);
    clEnqueueReadBuffer(queue, d_combinations, CL_TRUE, 0, cand_count * k * sizeof(u64), host_combinations, 0, NULL, NULL);

    // Поиск первого успешного (наибольшего) числа среди результатов
    int found_idx = -1;
    for (int i = 0; i < cand_count; i++) {
        if (results[i] == 1) {
            found_idx = i;
            break; // кандидаты отсортированы по убыванию (от N-1 вниз), первый найденный — наибольший
        }
    }

    if (found_idx != -1) {
        best_comb = malloc(k * sizeof(u64));
        if (!best_comb) {
            ret = MEM_ERR;
            goto cleanup;
        }
        for (size_t j = 0; j < k; j++) {
            best_comb[j] = host_combinations[found_idx * k + j];
        }
    } else {
        ret = NO_SUM;
    }

cleanup:
    if (d_primes) clReleaseMemObject(d_primes);
    if (d_candidates) clReleaseMemObject(d_candidates);
    if (d_results) clReleaseMemObject(d_results);
    if (d_combinations) clReleaseMemObject(d_combinations);
    if (kernel) clReleaseKernel(kernel);
    if (program) clReleaseProgram(program);
    if (queue) clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);

    free(candidates);
    free(results);
    free(host_combinations);
    free(primes);

    if (ret != NO_ERR) {
        if (best_comb) free(best_comb);
        return ret;
    }

    *M = best_comb;
    return NO_ERR;
}