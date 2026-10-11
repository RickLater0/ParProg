/*
OpenCL
42. Найти наибольшее число M, меньшее заданного числа N, 
которое можно представить суммой k различных простых чисел.
*/

#include "common.h"
#include "parallel.h"
#include "sequential.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

typedef lab_err_t (* max_prime_sum) (
    const uint64_t N, 
    const size_t k, 
    uint64_t **M
);

static struct timespec start;
static struct timespec end;

static void print_output(
    const uint64_t N, 
    const size_t k, 
    const uint64_t *M
);
static lab_err_t test(
    max_prime_sum test_func,
    const uint64_t N, 
    const size_t k, 
    uint64_t **M
);
static void handler(int sig);

int main(int argc, char *argv[])
{
    uint64_t N = 2;
    size_t k = 0;
    uint8_t is_seq = 0;
    uint8_t is_par = 1;
    size_t arg_ind = 1;
    uint64_t *M = NULL;
   

    if(argc != 3 && argc != 4)
    {
        printf("Usage: %s [-p|-s] <N> <k> \n\
            N - top edge\n\
            k - number of prime numbers in sum\n\
            Flags, may be both:\n\
            -p - parallel\n\
            -s - sequential\n",
            argv[0]
        );
        return -1;
    }

    if(argv[arg_ind][0] == '-')
    {
        for(size_t j = 1; argv[arg_ind][j] != '\0'; j++)
        {
            if(argv[arg_ind][j] == 's')
            {
                is_par = 0;
                is_seq = 1;
            }
            else if(argv[arg_ind][j] == 'p')
            {
                is_par = 1;
            }
            else
            {
                printf("Unknown flag option: '%c'\n", argv[arg_ind][j]);
                return -1;
            }
        }
        arg_ind++;
    }

    if(arg_ind < (size_t)argc)
    {
        N = (uint64_t)strtoull(argv[arg_ind], NULL, 10);
        arg_ind++;
    }

    if(arg_ind < (size_t)argc)
    {
        k = (size_t)strtoull(argv[arg_ind], NULL, 10);
        arg_ind++;
    }

    if(is_par)
    {
        printf("Running parallel...\n");
        test(parallel, N, k, &M);
    }
    if(is_seq)
    {
        printf("Running sequence...\n");
        test(sequence, N, k, &M);
    }

    free(M);
    return 0;
}

static void print_output(
    const uint64_t N, 
    const size_t k, 
    const uint64_t *M
)
{
    uint64_t sum = 0;
    double elapsed_time = 0.0;

    printf("Prime numbers which sum gives less than %lu:\n", N);
    for(size_t i = 0; i < k; i++)
    {
        printf("%lu", M[i]);
        printf(
            i != k - 1 ? 
            " + " : 
            " = "
        );
        sum += M[i];
    }
    printf("%lu\n", sum);

    elapsed_time = (double)(end.tv_sec - start.tv_sec) + 
                   (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Execution time: %.6f secs\n", elapsed_time);
}

static lab_err_t test(
    max_prime_sum test_func,
    const uint64_t N, 
    const size_t k, 
    uint64_t **M
)
{
    lab_err_t ret = NO_ERR;

    signal(SIGINT,  handler);
    signal(SIGTERM, handler);

    if (*M != NULL)
    {
        free(*M);
        *M = NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    ret = test_func(N, k, M);
    clock_gettime(CLOCK_MONOTONIC, &end);

    if(ret != NO_ERR)
    {
        printf("Error: %s\n", lab_err_string(ret));
        return ret;
    }

    print_output(N, k, *M);
    printf("\n");

    return ret;
}

static void handler(int sig)
{
    double elapsed_time = 0.0;
    (void) sig;

    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed_time = (double)(end.tv_sec - start.tv_sec) + 
                   (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("\nExecution time: %.6f secs. Interrupted\n", elapsed_time);
    exit(EXIT_FAILURE);
}