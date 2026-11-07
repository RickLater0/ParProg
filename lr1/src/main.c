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
#include <unistd.h>

typedef lab_err_t (* max_prime_sum) (
    const uint64_t N, 
    const size_t k, 
    uint64_t **M
);

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
static uint8_t is_diff(
    const uint64_t *M1, 
    const uint64_t *M2,
    const size_t k
);

static struct timespec start;
static struct timespec end;
static u8 use_color = 0;
#define C_RESET   ((use_color ? "\033[0m" : ""))
#define C_RED     ((use_color ? "\033[1;31m" : ""))
#define C_GREEN   ((use_color ? "\033[1;32m" : ""))

int main(int argc, char *argv[])
{
    use_color = isatty(STDOUT_FILENO);
    uint64_t N = 2;
    size_t k = 0;
    size_t arg_ind = 1;
    u8 has_diff = 1;
    uint64_t *M1 = NULL;
    uint64_t *M2 = NULL;

    if(argc != 3)
    {
        printf("Usage: %s <N> <k> \n\
            N - top edge\n\
            k - number of prime numbers in sum\n",
            argv[0]
        );
        return -1;
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


    printf("Parallel:\n");
    test(parallel, N, k, &M1);

    printf("Sequence:\n");
    test(sequence, N, k, &M2);

    has_diff = is_diff(M1, M2, k);
    printf("Has diff: %s%s%s\n", has_diff ? C_RED : C_GREEN, has_diff ? "true" : "false", C_RESET);

    free(M1);
    free(M2);
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
    printf("%lu < %lu\n", sum, N);

    elapsed_time = (double)(end.tv_sec - start.tv_sec) + 
                   (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Time: %.6f secs\n", elapsed_time);
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

    clock_gettime(CLOCK_MONOTONIC, &start);
    ret = test_func(N, k, M);
    clock_gettime(CLOCK_MONOTONIC, &end);

    if(ret != NO_ERR)
    {
        printf("%sError: %s%s\n", C_RED, lab_err_string(ret), C_RESET);
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

/**
 * @brief Проверяет два массива одинаковой размерности на соответствие
 * 
 * @param M1 первый массив
 * @param M2 второй массив
 * @param k количество элементов
 * @return uint8_t 1 - есть различия, иначе 0
 */
static uint8_t is_diff(
    const uint64_t *M1, 
    const uint64_t *M2,
    const size_t k
)
{
    uint8_t check = 0;

    for(size_t i = 0; i < k; i++)
    {
        if(M1[i] == M2[i])
        {
            continue;
        }
        check = 1;
        break;
    }

    return check;
}