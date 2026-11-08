#include "common.h"

#include <stdlib.h>
#include <string.h>

/* ---- контейнер результатов ---- */

lab_err_t result_init(result_t *r)
{
    if (!r) return ARG_ERR;
    r->entries  = NULL;
    r->count    = 0;
    r->capacity = 0;
    return NO_ERR;
}

lab_err_t result_append(result_t *r, triple_entry_t e)
{
    if (!r) return ARG_ERR;
    if (r->count == r->capacity) {
        u64 newcap = (r->capacity == 0) ? 128 : r->capacity * 2;
        triple_entry_t *tmp = realloc(r->entries,
                                      (size_t)newcap * sizeof(*tmp));
        if (!tmp) return MEM_ERR;
        r->entries  = tmp;
        r->capacity = newcap;
    }
    r->entries[r->count++] = e;
    return NO_ERR;
}

static int cmp_entry(const void *a, const void *b)
{
    const triple_entry_t *ea = (const triple_entry_t *)a;
    const triple_entry_t *eb = (const triple_entry_t *)b;
    if (ea->value < eb->value) return -1;
    if (ea->value > eb->value) return  1;
    if (ea->p1 < eb->p1) return -1;
    if (ea->p1 > eb->p1) return  1;
    if (ea->p2 < eb->p2) return -1;
    if (ea->p2 > eb->p2) return  1;
    if (ea->p3 < eb->p3) return -1;
    if (ea->p3 > eb->p3) return  1;
    return 0;
}

lab_err_t result_sort(result_t *r)
{
    if (!r) return ARG_ERR;
    if (r->count > 1) {
        qsort(r->entries, (size_t)r->count, sizeof(*r->entries), cmp_entry);
    }
    return NO_ERR;
}

void result_free(result_t *r)
{
    if (!r) return;
    free(r->entries);
    r->entries  = NULL;
    r->count    = 0;
    r->capacity = 0;
}

lab_err_t sieve(
    const u64 N,
    u64 **primes,
    size_t *p_sz
)
{
    u8 *primes_bool = NULL;
    size_t primes_len = 0;
    u64 *res = NULL;
    size_t ind = 0;

    if(!primes || !p_sz || N < 2)
    {
        return ARG_ERR;
    }

    primes_bool = calloc((size_t)(N + 1), sizeof(u8));
    if(!primes_bool)
    {
        return MEM_ERR;
    }

    for(u64 i = 0; i <= N; i++)
    {
        primes_bool[i] = 1;
    }
    primes_bool[0] = 0;
    primes_bool[1] = 0;

    for(u64 p = 2; p * p <= N; p++)
    {
        if(primes_bool[p] == 0)
        {
            continue;
        }
        for(u64 i = p * p; i <= N; i += p)
        {
            primes_bool[i] = 0;
        }
    }

    for(u64 i = 0; i <= N; i++){
        if(primes_bool[i] != 0)
        {
            primes_len++;
        }
    }

    res = calloc(primes_len, sizeof(u64));
    if(!res)
    {
        return MEM_ERR;
    }
    
    ind = 0;
    for(u64 i = 0; i <= N; i++){
        if(primes_bool[i] != 0)
        {
            res[ind] = i;
            ind++;
        }
    }

    free(primes_bool);
    *p_sz = primes_len;
    *primes = res;
    return NO_ERR;
}

/* ---- перебор троек (rank/size для распределения по p1) ---- */

lab_err_t enumerate_triples(
    u64 m, u64 n,
    u64 *primes, u64 np,
    u64 rank, u64 size,
    result_t *res
)
{
    if (!res || size == 0) return ARG_ERR;
    if (np < 3) return NO_ERR;

    for (u64 i = rank; i < np; i += size) {
        u64 p1 = primes[i];

        /* p1^3 > n ⇒ дальше смысла нет */
        if (p1 * p1 * p1 > n) break;

        for (u64 j = i + 1; j < np; j++) {
            u64 p2 = primes[j];
            if (p2 * p2 > n) break;                 /* p1*p2 > n        */

            u64 p1p2 = p1 * p2;
            
            if (p1p2 * (p2 + 1) > n) break;

            for (u64 k = j + 1; k < np; k++) {
                u64 p3 = primes[k];
                if (p1p2 * p3 > n) break;

                u64 x = p1p2 * p3;
                if (x < m) continue;

                triple_entry_t e = { x, p1, p2, p3 };
                lab_err_t err = result_append(res, e);
                if (err != NO_ERR) return err;
            }
        }
    }
    return NO_ERR;
}

const char *lab_err_string(lab_err_t err)
{
    switch (err) {
        case NO_ERR:              return "No error";
        case ARG_ERR:             return "Some args are NULL or invalid";
        case MEM_ERR:             return "Not enough memory";
        case PAR_NO_DEVICE:       return "MPI environment not initialized";
        case PAR_PROG_BUILD_ERR:  return "Parallel build failed";
        case PAR_INIT_ERR:        return "MPI init failed";
        case PAR_MEM_ERR:         return "MPI buffer allocation failed";
        case PAR_ARG_ERR:         return "Parallel argument error";
        case PAR_EXEC_ERR:        return "Parallel execution failed";
        default:                  return MODULE_NAME " Unknown error";
    }
}