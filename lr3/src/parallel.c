#include "parallel.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

solution p_solution = parallel;
struct timespec par_start = {0};
struct timespec par_end   = {0};

static int g_rank = 0;
static int g_size = 1;

static int cmp_u64_asc(
    const void *a,
    const void *b
);

static u64 isqrt_u64(u64 x);

static u64 upper_bound_u64(
    const u64 *s,
    u64 n_split,
    u64 v
);

static lab_err_t parallel_sieve(
    u64 limit,
    int rank,
    int size,
    u64 **primes_out,
    u64 *count_out
);

static lab_err_t parallel_sort(
    int size,
    result_t *res
);

static lab_err_t gather_to_root(
    int rank,
    int size,
    result_t *res
);

lab_err_t parallel_init(
    int *argc,
    char ***argv
)
{
    if (MPI_Init(argc, argv) != MPI_SUCCESS)
    {
        return PAR_INIT_ERR;
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &g_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &g_size);
    return NO_ERR;
}

lab_err_t parallel_finalize(void)
{
    MPI_Finalize();
    return NO_ERR;
}

int parallel_rank(void)
{
    return g_rank;
}

int parallel_size(void)
{
    return g_size;
}

lab_err_t parallel_broadcast_input(
    u64 *m,
    u64 *n,
    int *ok
)
{
    MPI_Bcast(ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!*ok)
    {
        return NO_ERR;
    }
    MPI_Bcast(m, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(n, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    return NO_ERR;
}

lab_err_t parallel_sync_err(
    lab_err_t err
)
{
    int val = (int)err;

    MPI_Bcast(&val, 1, MPI_INT, 0, MPI_COMM_WORLD);
    return (lab_err_t)val;
}

static int cmp_u64_asc(
    const void *a,
    const void *b
)
{
    u64 x = *(const u64 *)a;
    u64 y = *(const u64 *)b;
    int result = 0;

    if (x != y)
    {
        result = (x < y) ? -1 : 1;
    }
    return result;
}

static u64 isqrt_u64(u64 x)
{
    u64 r = 0;

    if (x == 0)
    {
        return 0;
    }
    r = (u64)sqrt((double)x);
    while (r > 0 && r > x / r)
    {
        r--;
    }
    while ((r + 1) <= x / (r + 1))
    {
        r++;
    }
    return r;
}

static u64 upper_bound_u64(
    const u64 *s,
    u64 n_split,
    u64 v
)
{
    u64 lo = 0;
    u64 hi = n_split;

    while (lo < hi)
    {
        u64 mid = lo + (hi - lo) / 2;
        if (v < s[mid])
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return lo;
}

static lab_err_t parallel_sieve(
    u64 limit,
    int rank,
    int size,
    u64 **primes_out,
    u64 *count_out
)
{
    u64 sq = 0;
    u64 lo = 0;
    u64 span = 0;
    u64 chunk = 0;
    u64 my_lo = 0;
    u64 my_hi = 0;
    u64 my_len = 0;
    u64 my_count = 0;
    u64 total_primes = 0;
    u64 *small = NULL;
    u64 *all = NULL;
    u64 *lbuf = NULL;
    u64 *counts = NULL;
    u8 *flags = NULL;
    size_t ns = 0;
    int *rc = NULL;
    int *dp = NULL;
    lab_err_t err = NO_ERR;

    *primes_out = NULL;
    *count_out  = 0;
    if (limit < 2)
    {
        return NO_ERR;
    }

    sq = isqrt_u64(limit);
    if (sq < 1)
    {
        sq = 1;
    }

    if (rank == 0)
    {
        err = sieve(sq, &small, &ns);
        if (err != NO_ERR)
        {
            return err;
        }
    }

    MPI_Bcast(&ns, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    if (rank != 0 && ns > 0)
    {
        small = malloc((size_t)ns * sizeof(u64));
        if (!small)
        {
            return PAR_MEM_ERR;
        }
    }
    if (ns > 0)
    {
        MPI_Bcast(small, (int)ns, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    }

    lo = sq + 1;
    span = (limit >= lo) ? (limit - lo + 1) : 0;
    chunk = (span + (u64)size - 1) / (u64)size;
    if (chunk == 0)
    {
        chunk = 1;
    }

    my_lo = lo + (u64)rank * chunk;
    my_hi = my_lo + chunk;
    if (my_hi > limit + 1)
    {
        my_hi = limit + 1;
    }
    if (my_lo > limit + 1)
    {
        my_lo = limit + 1;
    }
    my_len = (my_hi > my_lo) ? (my_hi - my_lo) : 0;

    if (my_len > 0)
    {
        flags = malloc(my_len);
        if (!flags)
        {
            free(small);
            return PAR_MEM_ERR;
        }
        memset(flags, 1, my_len);
        for (u64 i = 0; i < ns; i++)
        {
            u64 p = small[i];
            u64 pp = p * p;
            u64 start = my_lo + ((p - (my_lo % p)) % p);
            if (start < pp)
            {
                start = pp;
            }
            for (u64 j = start; j < my_hi; j += p)
            {
                flags[j - my_lo] = 0;
                if (my_hi - j <= p)
                {
                    break;
                }
            }
        }
        for (u64 j = 0; j < my_len; j++)
        {
            if (flags[j])
            {
                my_count++;
            }
        }
    }

    if (rank == 0)
    {
        counts = malloc((size_t)size * sizeof(u64));
        rc     = malloc((size_t)size * sizeof(int));
        dp     = malloc((size_t)size * sizeof(int));
        if (!counts || !rc || !dp)
        {
            free(small);
            free(flags);
            free(counts);
            free(rc);
            free(dp);
            return PAR_MEM_ERR;
        }
    }
    MPI_Gather(&my_count, 1, MPI_UINT64_T,
               counts, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    total_primes = ns;
    if (rank == 0)
    {
        u64 acc = 0;
        for (int i = 0; i < size; i++)
        {
            rc[i] = (int)(counts[i] * sizeof(u64));
            dp[i] = (int)acc;
            acc += counts[i] * sizeof(u64);
            total_primes += counts[i];
        }
    }
    if (rank != 0)
    {
        rc = malloc((size_t)size * sizeof(int));
        dp = malloc((size_t)size * sizeof(int));
        if (!rc || !dp)
        {
            free(small);
            free(flags);
            free(counts);
            free(rc);
            free(dp);
            return PAR_MEM_ERR;
        }
    }
    MPI_Bcast(&total_primes, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(rc, size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(dp, size, MPI_INT, 0, MPI_COMM_WORLD);

    if (my_count > 0)
    {
        u64 idx = 0;
        lbuf = malloc((size_t)my_count * sizeof(u64));
        if (!lbuf)
        {
            free(small);
            free(flags);
            free(counts);
            free(rc);
            free(dp);
            return PAR_MEM_ERR;
        }
        for (u64 j = 0; j < my_len; j++)
        {
            if (flags[j])
            {
                lbuf[idx++] = my_lo + j;
            }
        }
    }
    free(flags);
    flags = NULL;

    all = malloc((size_t)total_primes * sizeof(u64));
    if (!all && total_primes > 0)
    {
        free(small);
        free(lbuf);
        free(counts);
        free(rc);
        free(dp);
        return PAR_MEM_ERR;
    }
    if (ns > 0)
    {
        memcpy(all, small, (size_t)ns * sizeof(u64));
    }

    {
        void *sb = lbuf ? (void *)lbuf : (void *)&my_count;
        MPI_Allgatherv(sb, (int)(my_count * sizeof(u64)), MPI_BYTE,
                       all + ns, rc, dp, MPI_BYTE, MPI_COMM_WORLD);
    }

    free(small);
    free(lbuf);
    free(counts);
    free(rc);
    free(dp);

    *primes_out = all;
    *count_out  = total_primes;
    return NO_ERR;
}

static lab_err_t parallel_sort(
    int size,
    result_t *res
)
{
    lab_err_t err = NO_ERR;
    const u64 p       = (u64)size;
    const u64 n_split = p - 1;
    u64 n_local       = 0;
    u64 total_split   = 0;
    u64 tot_send      = 0;
    u64 tot_recv      = 0;
    u64 n_total       = 0;

    u64 *local_split = NULL;
    u64 *all_split   = NULL;
    u64 *g_split     = NULL;
    u64 *bin_send    = NULL;
    u64 *bin_recv    = NULL;
    u64 *off         = NULL;
    u64 *rsz         = NULL;
    u64 *ridx        = NULL;
    int *sc          = NULL;
    int *sd          = NULL;
    int *rc          = NULL;
    int *rd          = NULL;
    triple_entry_t *sbuf = NULL;
    triple_entry_t *rbuf = NULL;
    const triple_entry_t **runs = NULL;

    err = result_sort(res);
    if (err != NO_ERR)
    {
        return err;
    }
    if (size <= 1)
    {
        return NO_ERR;
    }

    n_local = res->count;

    if (n_split > 0)
    {
        local_split = malloc((size_t)n_split * sizeof(u64));
        if (!local_split)
        {
            err = PAR_MEM_ERR;
            goto end;
        }
        for (u64 i = 0; i < n_split; i++)
        {
            if (n_local == 0)
            {
                local_split[i] = UINT64_MAX;
            }
            else
            {
                u64 idx = (n_local * (i + 1)) / p;
                if (idx >= n_local)
                {
                    idx = n_local - 1;
                }
                local_split[i] = res->entries[idx].value;
            }
        }
    }

    total_split = p * n_split;
    if (total_split > 0)
    {
        int nb = (int)(n_split * sizeof(u64));
        all_split = malloc((size_t)total_split * sizeof(u64));
        if (!all_split)
        {
            err = PAR_MEM_ERR;
            goto end;
        }
        MPI_Allgather(local_split, nb, MPI_BYTE,
                      all_split, nb, MPI_BYTE, MPI_COMM_WORLD);
    }

    if (n_split > 0)
    {
        g_split = malloc((size_t)n_split * sizeof(u64));
        if (!g_split)
        {
            err = PAR_MEM_ERR;
            goto end;
        }
        qsort(all_split, (size_t)total_split, sizeof(u64), cmp_u64_asc);
        for (u64 i = 0; i < n_split; i++)
        {
            u64 idx = (total_split * (i + 1)) / p;
            if (idx >= total_split)
            {
                idx = total_split - 1;
            }
            g_split[i] = all_split[idx];
        }
    }

    bin_send = calloc((size_t)p, sizeof(u64));
    bin_recv = malloc((size_t)p * sizeof(u64));
    if (!bin_send || !bin_recv)
    {
        err = PAR_MEM_ERR;
        goto end;
    }

    for (u64 i = 0; i < n_local; i++)
    {
        u64 b = upper_bound_u64(g_split, n_split, res->entries[i].value);
        bin_send[b]++;
    }
    MPI_Alltoall(bin_send, 1, MPI_UINT64_T,
                 bin_recv, 1, MPI_UINT64_T, MPI_COMM_WORLD);

    sc = malloc((size_t)p * sizeof(int));
    sd = malloc((size_t)p * sizeof(int));
    rc = malloc((size_t)p * sizeof(int));
    rd = malloc((size_t)p * sizeof(int));
    if (!sc || !sd || !rc || !rd)
    {
        err = PAR_MEM_ERR;
        goto end;
    }

    tot_send = 0;
    tot_recv = 0;
    for (u64 i = 0; i < p; i++)
    {
        sc[i] = (int)(bin_send[i] * sizeof(triple_entry_t));
        sd[i] = (int)tot_send;
        tot_send += bin_send[i] * sizeof(triple_entry_t);
        rc[i] = (int)(bin_recv[i] * sizeof(triple_entry_t));
        rd[i] = (int)tot_recv;
        tot_recv += bin_recv[i] * sizeof(triple_entry_t);
    }

    off = calloc((size_t)p, sizeof(u64));
    if (!off)
    {
        err = PAR_MEM_ERR;
        goto end;
    }
    if (tot_send > 0)
    {
        sbuf = malloc(tot_send);
        if (!sbuf)
        {
            err = PAR_MEM_ERR;
            goto end;
        }
    }
    for (u64 i = 0; i < n_local; i++)
    {
        u64 b = upper_bound_u64(g_split, n_split, res->entries[i].value);
        u64 boff = sd[b] + off[b] * sizeof(triple_entry_t);
        off[b]++;
        memcpy((char *)sbuf + boff,
               &res->entries[i],
               sizeof(triple_entry_t));
    }

    if (tot_recv > 0)
    {
        rbuf = malloc(tot_recv);
        if (!rbuf)
        {
            err = PAR_MEM_ERR;
            goto end;
        }
    }
    {
        void *sb = sbuf ? (void *)sbuf : (void *)&tot_send;
        void *rb = rbuf ? (void *)rbuf : (void *)&tot_recv;
        MPI_Alltoallv(sb, sc, sd, MPI_BYTE,
                      rb, rc, rd, MPI_BYTE, MPI_COMM_WORLD);
    }

    n_total = tot_recv / sizeof(triple_entry_t);

    free(res->entries);
    res->entries  = (n_total > 0)
                  ? malloc((size_t)n_total * sizeof(triple_entry_t))
                  : NULL;
    res->count    = 0;
    res->capacity = n_total;
    if (n_total > 0 && !res->entries)
    {
        err = PAR_MEM_ERR;
        goto end;
    }

    runs = malloc((size_t)p * sizeof(*runs));
    rsz  = malloc((size_t)p * sizeof(u64));
    ridx = malloc((size_t)p * sizeof(u64));
    if (!runs || !rsz || !ridx)
    {
        err = PAR_MEM_ERR;
        goto end;
    }

    for (u64 i = 0; i < p; i++)
    {
        runs[i] = (const triple_entry_t *)((const char *)rbuf + rd[i]);
        rsz[i]  = bin_recv[i];
        ridx[i] = 0;
    }
    for (u64 k = 0; k < n_total; k++)
    {
        int best = -1;
        for (int i = 0; i < size; i++)
        {
            if (ridx[i] >= rsz[i])
            {
                continue;
            }
            if (best < 0 ||
                triple_entry_cmp(&runs[i][ridx[i]],
                                 &runs[best][ridx[best]]) < 0)
            {
                best = i;
            }
        }
        if (best < 0)
        {
            break;
        }
        res->entries[res->count++] = runs[best][ridx[best]++];
    }

end:
    free(local_split);
    free(all_split);
    free(g_split);
    free(bin_send);
    free(bin_recv);
    free(sc);
    free(sd);
    free(rc);
    free(rd);
    free(sbuf);
    free(rbuf);
    free(off);
    free(runs);
    free(rsz);
    free(ridx);
    return err;
}

static lab_err_t gather_to_root(
    int rank,
    int size,
    result_t *res
)
{
    u64 local = res->count;
    u64 total = 0;
    u64 *counts = NULL;
    int *rc = NULL;
    int *dp = NULL;
    triple_entry_t *all = NULL;

    if (rank == 0)
    {
        counts = malloc((size_t)size * sizeof(u64));
        rc     = malloc((size_t)size * sizeof(int));
        dp     = malloc((size_t)size * sizeof(int));
        if (!counts || !rc || !dp)
        {
            free(counts);
            free(rc);
            free(dp);
            return PAR_MEM_ERR;
        }
    }
    MPI_Gather(&local, 1, MPI_UINT64_T,
               counts, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        u64 acc = 0;
        for (int i = 0; i < size; i++)
        {
            rc[i] = (int)(counts[i] * sizeof(triple_entry_t));
            dp[i] = (int)acc;
            acc += counts[i] * sizeof(triple_entry_t);
            total += counts[i];
        }
        if (total > 0)
        {
            all = malloc((size_t)total * sizeof(triple_entry_t));
            if (!all)
            {
                free(counts);
                free(rc);
                free(dp);
                return PAR_MEM_ERR;
            }
        }
    }

    {
        void *sb = res->entries ? (void *)res->entries : (void *)&local;
        void *rb = all ? (void *)all : (void *)&total;
        MPI_Gatherv(sb, (int)(local * sizeof(triple_entry_t)), MPI_BYTE,
                    rb, rc, dp, MPI_BYTE, 0, MPI_COMM_WORLD);
    }

    if (rank == 0)
    {
        free(res->entries);
        res->entries  = all;
        res->count    = total;
        res->capacity = total;
    }
    else
    {
        result_free(res);
    }

    free(counts);
    free(rc);
    free(dp);
    return NO_ERR;
}

lab_err_t parallel(
    u64 m,
    u64 n,
    result_t *res
)
{
    lab_err_t err = NO_ERR;
    u64 *primes = NULL;
    u64 np = 0;

    err = result_init(res);
    if (err != NO_ERR)
    {
        return err;
    }

    // Старт таймера: включаем полезную работу (решето + перебор + сортировка),
    // но исключаем накладные расходы (сбор результатов в gather_to_root)
    clock_gettime(CLOCK_MONOTONIC, &par_start);

    err = parallel_sieve(n / 6 + 10, g_rank, g_size, &primes, &np);
    if (err != NO_ERR)
    {
        goto end;
    }

    err = enumerate_triples(m, n, primes, np, (u64)g_rank, (u64)g_size, res);
    if (err != NO_ERR)
    {
        goto end;
    }

    err = parallel_sort(g_size, res);
    if (err != NO_ERR)
    {
        goto end;
    }

    // Конец таймера: gather_to_root — это накладные расходы на вывод,
    // их не засчитываем в время параллельного решения
    clock_gettime(CLOCK_MONOTONIC, &par_end);

    err = gather_to_root(g_rank, g_size, res);

end:
    free(primes);
    return err;
}