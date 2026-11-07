#include "common.h"
#include "parallel.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

static lab_err_t parallel_run(
    const graph_t *g1,
    const graph_t *g2,
    graph_t *ret_g1,
    graph_t *ret_g2,
    u8 *res
);

solution p_solution = parallel_run;
struct timespec par_start = {0};
struct timespec par_end = {0};

static int cl_initialized = 0;
static cl_context context = NULL;
static cl_command_queue queue = NULL;
static cl_program program = NULL;
static cl_kernel kernel_compat = NULL;
static cl_device_id device = NULL;

static const char *ocl_source = 
"#define u8 unsigned char\n"
"__kernel void vertex_compatibility(\n"
"    __global const u8 *m1,\n"
"    __global const u8 *m2,\n"
"    __global u8 *compat,\n"
"    int n)\n"
"{\n"
"    int i = get_global_id(0);\n"
"    int j = get_global_id(1);\n"
"    if(i >= n || j >= n) return;\n"
"\n"
"    int deg1 = 0;\n"
"    int deg2 = 0;\n"
"    for(int k = 0; k < n; k++)\n"
"    {\n"
"        if(m1[i * n + k]) deg1++;\n"
"        if(m2[j * n + k]) deg2++;\n"
"    }\n"
"\n"
"    if(deg1 == deg2)\n"
"    {\n"
"        compat[i * n + j] = 1;\n"
"    }\n"
"    else\n"
"    {\n"
"        compat[i * n + j] = 0;\n"
"    }\n"
"}\n";

static lab_err_t init_opencl_once(void)
{
    if(cl_initialized)
    {
        return NO_ERR;
    }

    cl_platform_id platform = NULL;
    cl_int err = 0;

    if(clGetPlatformIDs(1, &platform, NULL) != CL_SUCCESS)
    {
        return PAR_NO_DEVICE;
    }

    if(clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL) != CL_SUCCESS)
    {
        if(clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL) != CL_SUCCESS)
        {
            return PAR_NO_DEVICE;
        }
    }

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if(!context)
    {
        return PAR_INIT_ERR;
    }

#ifdef CL_VERSION_2_0
    queue = clCreateCommandQueueWithProperties(context, device, 0, &err);
#else
    queue = clCreateCommandQueue(context, device, 0, &err);
#endif
    if(!queue)
    {
        return PAR_INIT_ERR;
    }

    program = clCreateProgramWithSource(context, 1, &ocl_source, NULL, &err);
    if(!program)
    {
        return PAR_PROG_BUILD_ERR;
    }

    if(clBuildProgram(program, 1, &device, NULL, NULL, NULL) != CL_SUCCESS)
    {
        return PAR_PROG_BUILD_ERR;
    }

    kernel_compat = clCreateKernel(program, "vertex_compatibility", &err);
    if(!kernel_compat)
    {
        return PAR_EXEC_ERR;
    }

    cl_initialized = 1;
    return NO_ERR;
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
    u8 n = src->n;

    if(!dst || !src || !mapping)
    {
        return ARG_ERR;
    }

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

static int iso_backtrack_smart(
    const graph_t *g1,
    const graph_t *g2,
    const u8 *compat,
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

        if(!compat[i * n + j])
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

        if(iso_backtrack_smart(g1, g2, compat, mapping, used, depth + 1))
        {
            return 1;
        }

        used[j] = 0;
    }

    return 0;
}

lab_err_t parallel(
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
    u8 *compat_matrix = NULL;
    u8 *flat_m1 = NULL;
    u8 *flat_m2 = NULL;
    cl_mem buf_m1 = NULL;
    cl_mem buf_m2 = NULL;
    cl_mem buf_compat = NULL;
    int found = 0;
    size_t matrix_size = 0;
    cl_int cl_err = 0;
    cl_int n_int = 0;
    size_t global_work_size[2] = {0, 0};

    par_start.tv_sec = 0;
    par_start.tv_nsec = 0;
    par_end.tv_sec = 0;
    par_end.tv_nsec = 0;

    if(!g1 || !g2 || !ret_g1 || !ret_g2 || !res)
    {
        return ARG_ERR;
    }

    *res = 0;
    ret_g1->n = 0;
    ret_g1->matrix = NULL;
    ret_g2->n = 0;
    ret_g2->matrix = NULL;

    n = g1->n;

    if(g1->n == g2->n && n > 0)
    {
        matrix_size = (size_t)n * n * sizeof(u8);
        flat_m1 = malloc(matrix_size);
        flat_m2 = malloc(matrix_size);
        compat_matrix = calloc(n * n, sizeof(u8));
        if(!flat_m1 || !flat_m2 || !compat_matrix)
        {
            ret = MEM_ERR;
            goto end;
        }

        for(u8 i = 0; i < n; i++)
        {
            memcpy(flat_m1 + i * n, g1->matrix[i], n);
            memcpy(flat_m2 + i * n, g2->matrix[i], n);
        }

        buf_m1 = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, matrix_size, flat_m1, &cl_err);
        buf_m2 = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, matrix_size, flat_m2, &cl_err);
        buf_compat = clCreateBuffer(context, CL_MEM_WRITE_ONLY, matrix_size, NULL, &cl_err);

        if(!buf_m1 || !buf_m2 || !buf_compat)
        {
            ret = PAR_MEM_ERR;
            goto end;
        }

        n_int = (cl_int)n;
        clSetKernelArg(kernel_compat, 0, sizeof(cl_mem), &buf_m1);
        clSetKernelArg(kernel_compat, 1, sizeof(cl_mem), &buf_m2);
        clSetKernelArg(kernel_compat, 2, sizeof(cl_mem), &buf_compat);
        clSetKernelArg(kernel_compat, 3, sizeof(n_int), &n_int);

        global_work_size[0] = n;
        global_work_size[1] = n;
    }

    clock_gettime(CLOCK_MONOTONIC, &par_start);

    if(g1->n != g2->n)
    {
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR) goto end;
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR) goto end;
        goto end;
    }

    if(count_ones(g1) != count_ones(g2))
    {
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR) goto end;
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR) goto end;
        goto end;
    }

    if(!degree_equal(g1, g2))
    {
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR) goto end;
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR) goto end;
        goto end;
    }

    if(n == 0)
    {
        *res = 1;
        goto end;
    }

    cl_err = clEnqueueNDRangeKernel(queue, kernel_compat, 2, NULL, global_work_size, NULL, 0, NULL, NULL);
    if(cl_err != CL_SUCCESS)
    {
        ret = PAR_EXEC_ERR;
        goto end;
    }

    cl_err = clEnqueueReadBuffer(queue, buf_compat, CL_TRUE, 0, matrix_size, compat_matrix, 0, NULL, NULL);
    if(cl_err != CL_SUCCESS)
    {
        ret = PAR_MEM_ERR;
        goto end;
    }

    mapping = calloc(n, sizeof(u8));
    used = calloc(n, sizeof(u8));
    if(!mapping || !used)
    {
        ret = MEM_ERR;
        goto end;
    }

    found = iso_backtrack_smart(g1, g2, compat_matrix, mapping, used, 0);

    if(found)
    {
        *res = 1;
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR) goto end;
        if((ret = copy_graph_permuted(ret_g2, g2, mapping)) != NO_ERR) goto end;
    }
    else
    {
        *res = 0;
        if((ret = copy_graph(ret_g1, g1)) != NO_ERR) goto end;
        if((ret = copy_graph(ret_g2, g2)) != NO_ERR) goto end;
    }

    end:
        if(par_start.tv_sec != 0 || par_start.tv_nsec != 0)
        {
            clock_gettime(CLOCK_MONOTONIC, &par_end);
        }

        if(buf_m1) clReleaseMemObject(buf_m1);
        if(buf_m2) clReleaseMemObject(buf_m2);
        if(buf_compat) clReleaseMemObject(buf_compat);

        free(flat_m1);
        free(flat_m2);
        free(compat_matrix);
        free(mapping);
        free(used);

        return ret;
}

static lab_err_t parallel_run(
    const graph_t *g1,
    const graph_t *g2,
    graph_t *ret_g1,
    graph_t *ret_g2,
    u8 *res
)
{
    lab_err_t err = init_opencl_once();
    if(err != NO_ERR)
    {
        return err;
    }

    return parallel(g1, g2, ret_g1, ret_g2, res);
}