#include "common.h"

#include <stdlib.h>
#include <string.h>

lab_err_t result_init(result_t *r)
{
    if (!r)
    {
        return ARG_ERR;
    }
    r->entries  = NULL;
    r->count    = 0;
    r->capacity = 0;
    return NO_ERR;
}

lab_err_t result_append(
    result_t *r,
    triple_entry_t e
)
{
    if (!r)
    {
        return ARG_ERR;
    }
    if (r->count == r->capacity)
    {
        u64 newcap = (r->capacity == 0) ? 128 : r->capacity * 2;
        triple_entry_t *tmp = NULL;

        tmp = realloc(r->entries, (size_t)newcap * sizeof(*tmp));
        if (!tmp)
        {
            return MEM_ERR;
        }
        r->entries  = tmp;
        r->capacity = newcap;
    }
    r->entries[r->count++] = e;
    return NO_ERR;
}

int triple_entry_cmp(
    const void *a,
    const void *b
)
{
    const triple_entry_t *ea = (const triple_entry_t *)a;
    const triple_entry_t *eb = (const triple_entry_t *)b;
    int result = 0;

    if (ea->value != eb->value)
    {
        result = (ea->value < eb->value) ? -1 : 1;
    }
    else if (ea->p1 != eb->p1)
    {
        result = (ea->p1 < eb->p1) ? -1 : 1;
    }
    else if (ea->p2 != eb->p2)
    {
        result = (ea->p2 < eb->p2) ? -1 : 1;
    }
    else if (ea->p3 != eb->p3)
    {
        result = (ea->p3 < eb->p3) ? -1 : 1;
    }
    return result;
}

lab_err_t result_sort(result_t *r)
{
    if (!r)
    {
        return ARG_ERR;
    }
    if (r->count > 1)
    {
        qsort(r->entries, (size_t)r->count,
              sizeof(*r->entries), triple_entry_cmp);
    }
    return NO_ERR;
}

void result_free(result_t *r)
{
    if (!r)
    {
        return;
    }
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
    u8 *composite = NULL;
    size_t primes_len = 0;
    u64 *res = NULL;
    size_t idx = 0;

    if (!primes || !p_sz || N < 2)
    {
        return ARG_ERR;
    }
    if (N >= SIZE_MAX)
    {
        return ARG_ERR;
    }

    composite = calloc((size_t)(N + 1), sizeof(u8));
    if (!composite)
    {
        return MEM_ERR;
    }
    composite[0] = 1;
    composite[1] = 1;

    for (u64 p = 2; p <= N / p; p++)
    {
        if (composite[p])
        {
            continue;
        }
        for (u64 i = p * p; i <= N; i += p)
        {
            composite[i] = 1;
            if (N - i < p)
            {
                break;
            }
        }
    }

    for (u64 i = 2; i <= N; i++)
    {
        if (!composite[i])
        {
            primes_len++;
        }
    }

    res = malloc(primes_len * sizeof(u64));
    if (!res && primes_len > 0)
    {
        free(composite);
        return MEM_ERR;
    }

    idx = 0;
    for (u64 i = 2; i <= N; i++)
    {
        if (!composite[i])
        {
            res[idx++] = i;
        }
    }

    free(composite);
    *p_sz   = primes_len;
    *primes = res;
    return NO_ERR;
}

lab_err_t enumerate_triples(
    u64 m,
    u64 n,
    u64 *primes,
    u64 np,
    u64 rank,
    u64 size,
    result_t *res
)
{
    if (!res || size == 0)
    {
        return ARG_ERR;
    }
    if (np < 3)
    {
        return NO_ERR;
    }

    // деление для защиты от переполнения u64
    for (u64 i = rank; i < np; i += size)
    {
        u64 p1 = primes[i];

        if (p1 > n / p1 / p1)
        {
            break;
        }

        for (u64 j = i + 1; j < np; j++)
        {
            u64 p2 = primes[j];
            u64 p1p2 = 0;

            if (p2 > n / p2)
            {
                break;
            }

            p1p2 = p1 * p2;

            if (p1p2 > n / (p2 + 1))
            {
                break;
            }

            for (u64 k = j + 1; k < np; k++)
            {
                u64 p3 = primes[k];
                u64 x = 0;
                lab_err_t err = NO_ERR;

                if (p1p2 > n / p3)
                {
                    break;
                }

                x = p1p2 * p3;
                if (x < m)
                {
                    continue;
                }

                err = result_append(res, (triple_entry_t){ x, p1, p2, p3 });
                if (err != NO_ERR)
                {
                    return err;
                }
            }
        }
    }
    return NO_ERR;
}

const char *lab_err_string(lab_err_t err)
{
    switch (err)
    {
    case NO_ERR:
        return "No error";
    case ARG_ERR:
        return "Some args are NULL or invalid";
    case MEM_ERR:
        return "Not enough memory";
    case PAR_NO_DEVICE:
        return "MPI environment not initialized";
    case PAR_PROG_BUILD_ERR:
        return "Parallel build failed";
    case PAR_INIT_ERR:
        return "MPI init failed";
    case PAR_MEM_ERR:
        return "MPI buffer allocation failed";
    case PAR_ARG_ERR:
        return "Parallel argument error";
    case PAR_EXEC_ERR:
        return "Parallel execution failed";
    default:
        return MODULE_NAME " Unknown error";
    }
}