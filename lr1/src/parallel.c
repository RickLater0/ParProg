#include "common.h"
#include "parallel.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifdef _OPENMP
#include <omp.h>
#endif

static int check_single_target(
    const u64 *primes,
    size_t p_sz,
    size_t k,
    size_t start_idx,
    u64 current_sum,
    u64 target,
    u64 *current_comb,
    size_t comb_idx,
    u64 *best_comb
)
{
    if (k == 0)
    {
        if (current_sum == target)
        {
            for (size_t i = 0; i < comb_idx; i++)
            {
                best_comb[i] = current_comb[i];
            }
            return 1;
        }
        return 0;
    }

    if (current_sum >= target)
    {
        return 0;
    }

    for (size_t i = start_idx; i < p_sz; i++)
    {
        if (p_sz - i < k)
        {
            break;
        }
        if (current_sum + primes[i] > target)
        {
            break; 
        }

        current_comb[comb_idx] = primes[i];
        if (check_single_target(primes, p_sz, k - 1, i + 1, current_sum + primes[i], target, current_comb, comb_idx + 1, best_comb))
        {
            return 1;
        }
    }
    return 0;
}

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

    u64 total_candidates = (current_target >= min_possible_sum) ? (current_target - min_possible_sum + 1) : 0;
    if (total_candidates == 0)
    {
        free(primes);
        return NO_SUM;
    }

    u64 max_found_target = 0;
    int found_flag = 0;

    // Идем блоками сверху вниз, чтобы гарантированно найти наибольшее число
    #pragma omp parallel
    {
        u64 *local_comb = malloc(k * sizeof(u64));
        u64 local_max_target = 0;
        int local_found = 0;

        #pragma omp for schedule(dynamic, 32)
        for (u64 idx = 0; idx < total_candidates; idx++)
        {
            // Если мы уже нашли решение выше текущего блока, пропускаем
            if (found_flag)
            {
                continue;
            }

            u64 target = current_target - idx;
            if (local_comb)
            {
                if (check_single_target(primes, p_sz, k, 0, 0, target, local_comb, 0, local_comb))
                {
                    if (target > local_max_target)
                    {
                        local_max_target = target;
                        local_found = 1;
                    }
                }
            }
        }

        if (local_found)
        {
            #pragma omp critical
            {
                if (!found_flag || local_max_target > max_found_target)
                {
                    max_found_target = local_max_target;
                    found_flag = 1;
                    if (!best_comb)
                    {
                        best_comb = malloc(k * sizeof(u64));
                    }
                    if (best_comb && local_comb)
                    {
                        for (size_t j = 0; j < k; j++)
                        {
                            best_comb[j] = local_comb[j];
                        }
                    }
                }
            }
        }
        free(local_comb);
    }

    // Если нашли максимальный таргет, перепроверим и запишем точную комбинацию для него
    if (found_flag)
    {
        // Пересберем комбинацию для max_found_target на всякий случай через sequential-подобный вызов
        u64 *exact_comb = malloc(k * sizeof(u64));
        if (exact_comb)
        {
            if (check_single_target(primes, p_sz, k, 0, 0, max_found_target, exact_comb, 0, exact_comb))
            {
                free(best_comb);
                best_comb = exact_comb;
            }
            else
            {
                free(exact_comb);
            }
        }
    }

    free(primes);

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