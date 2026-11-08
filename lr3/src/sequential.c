#include "sequential.h"

#include <stdlib.h>
#include <time.h>

solution s_solution = sequential;
struct timespec seq_start = {0};
struct timespec seq_end   = {0};

lab_err_t sequential(u64 m, u64 n, result_t *res)
{
    lab_err_t err = NO_ERR;
    u64 *primes = NULL;
    u64  np = 0;

    clock_gettime(CLOCK_MONOTONIC, &seq_start);

    result_init(res);

    /* максимально возможный простой множитель p3 <= n/(2*3) */
    u64 limit = n / 6 + 10;

    err = sieve(limit, &primes, &np);
    if (err != NO_ERR) goto end;

    err = enumerate_triples(m, n, primes, np, 0, 1, res);
    if (err != NO_ERR) goto end;

    err = result_sort(res);

end:
    free(primes);
    clock_gettime(CLOCK_MONOTONIC, &seq_end);
    return err;
}