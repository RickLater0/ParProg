#include "sequential.h"
#include "common.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static void find_sum(
    const u64 *primes,
    size_t primes_sz,
    size_t k,
    size_t start_idx,
    u64 current_sum,
    const u64 N,
    u64 *current_comb,
    size_t comb_idx,
    u64 *max_sum,
    u64 *best_comb
);

lab_err_t sequence(
    const u64 N,
    const size_t k,
    u64 **M
)
{
    u64 *primes = NULL;
    size_t p_sz = 0;
    u64 *best_comb = NULL;
    u64 *current_comb = NULL;
    u64 max_sum = 0;
    lab_err_t ret = NO_ERR;

    if(!M)
    {
        return ARG_ERR;
    }

    if((ret = sieve(N, &primes, &p_sz)) != NO_ERR)
    {
        return ret;
    }

    if(k > p_sz)
    {
        ret = LARGE_K;
        goto end;
    }

    best_comb = calloc((size_t)k, sizeof(u64));
    current_comb = calloc((size_t)k, sizeof(u64));
    if(!best_comb || !current_comb)
    {
        ret = MEM_ERR;
        goto end;
    }

    find_sum(
        primes,
        p_sz,
        k,
        0,
        0,
        N,
        current_comb,
        0,
        &max_sum,
        best_comb
    );

    if(max_sum == 0)
    {
        ret = NO_SUM;
        goto end;
    }

    *M = best_comb;
    best_comb = NULL;

    end:
        free(primes);
        free(best_comb);
        free(current_comb);
        return ret;
}



/**
 * @brief рекурсивная функция поиска максимальной суммы простых чисел, меньшей N
 * 
 * @param primes пул простых чисел
 * @param primes_sz размер пула
 * @param k -ое слагаемое (0 значит что сумма закончилась)
 * @param start_idx начальный индекс в пуле простых чисел для текущего шага рекурсии
 * @param current_sum текущая сумма выбранных чисел в текущей комбинации
 * @param N верхняя граница
 * @param current_comb временный массив для построения текущей комбинации
 * @param comb_idx текущий индекс заполнения временного массива комбинации
 * @param max_sum указатель на переменную, хранящую текущую наибольшую найденную сумму
 * @param best_comb массив для сохранения наилучшей найденной комбинации
 */
void find_sum(
    const u64 *primes,
    size_t primes_sz,
    size_t k,
    size_t start_idx,
    u64 current_sum,
    const u64 N,
    u64 *current_comb,
    size_t comb_idx,
    u64 *max_sum,
    u64 *best_comb
)
{
    if(k == 0)
    {
        if(current_sum >= N || current_sum <= *max_sum)
        {
            return;
        }

        *max_sum = current_sum;
        for(size_t i = 0; i < comb_idx; i++)
        {
            best_comb[i] = current_comb[i];
        }
        return;
    }
    if(current_sum >= N)
    {
        return;
    }
    for(size_t i = start_idx; i < primes_sz; i++)
    {
        if(primes_sz - i < k)
        {
            break;
        }
        current_comb[comb_idx] = primes[i];
        find_sum(
            primes,
            primes_sz,
            k - 1,
            i + 1,
            current_sum + primes[i],
            N,
            current_comb,
            comb_idx + 1,
            max_sum,
            best_comb
        );
    }
}