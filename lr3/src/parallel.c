#include "parallel.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

solution p_solution = parallel;
struct timespec par_start = {0};
struct timespec par_end   = {0};

/* ------------------------------------------------------------------ */
/*  общие утилиты                                                     */
/* ------------------------------------------------------------------ */

static int cmp_triple(const void *a, const void *b)
{
    const triple_entry_t *ea = (const triple_entry_t *)a;
    const triple_entry_t *eb = (const triple_entry_t *)b;
    if (ea->value != eb->value) return (ea->value < eb->value) ? -1 : 1;
    if (ea->p1    != eb->p1)    return (ea->p1    < eb->p1)    ? -1 : 1;
    if (ea->p2    != eb->p2)    return (ea->p2    < eb->p2)    ? -1 : 1;
    if (ea->p3    != eb->p3)    return (ea->p3    < eb->p3)    ? -1 : 1;
    return 0;
}

static int cmp_u64_asc(const void *a, const void *b)
{
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static u64 isqrt_u64(u64 x)
{
    if (x == 0) return 0;
    u64 r = (u64)sqrt((double)x);
    while (r > 0 && r > x / r) r--;
    while ((r + 1) <= x / (r + 1)) r++;
    return r;
}

/* бинарный поиск: индекс первой позиции splitter[j] > v
 * (если все <= v — вернёт n_split) */
static u64 upper_bound_u64(const u64 *s, u64 n_split, u64 v)
{
    u64 lo = 0, hi = n_split;
    while (lo < hi) {
        u64 mid = lo + (hi - lo) / 2;
        if (v < s[mid]) hi = mid;
        else            lo = mid + 1;
    }
    return lo;
}

/* ------------------------------------------------------------------ */
/*  ПАРАЛЛЕЛЬНОЕ РЕШЕТО                                               */
/* ------------------------------------------------------------------ */

static lab_err_t parallel_sieve(u64 limit, int rank, int size,
                                u64 **primes_out, u64 *count_out)
{
    *primes_out = NULL;
    *count_out  = 0;
    if (limit < 2) return NO_ERR;

    u64 sq = isqrt_u64(limit);
    if (sq < 1) sq = 1;

    /* --- 1. rank 0 строит малые простые <= sq --- */
    u64 *small = NULL;
    size_t ns = 0;
    if (rank == 0) {
        lab_err_t e = sieve(sq, &small, &ns);
        if (e != NO_ERR) return e;
    }

    /* --- 2. broadcast малых простых --- */
    MPI_Bcast(&ns, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    if (rank != 0 && ns > 0) {
        small = malloc((size_t)ns * sizeof(u64));
        if (!small) return PAR_MEM_ERR;
    }
    if (ns > 0) MPI_Bcast(small, (int)ns, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    /* --- 3. каждый ранг просеивает свой блок (sq, limit] --- */
    u64 lo = sq + 1;
    u64 span = (limit >= lo) ? (limit - lo + 1) : 0;
    u64 chunk = (span + (u64)size - 1) / (u64)size;
    if (chunk == 0) chunk = 1;

    u64 my_lo = lo + (u64)rank * chunk;
    u64 my_hi = my_lo + chunk;
    if (my_hi > limit + 1) my_hi = limit + 1;
    if (my_lo > limit + 1) my_lo = limit + 1;
    u64 my_len = (my_hi > my_lo) ? (my_hi - my_lo) : 0;

    u8 *flags = NULL;
    u64 my_count = 0;
    if (my_len > 0) {
        flags = malloc(my_len);
        if (!flags) { free(small); return PAR_MEM_ERR; }
        memset(flags, 1, my_len);
        for (u64 i = 0; i < ns; i++) {
            u64 p = small[i];
            u64 pp = p * p;                  /* <= sq^2 <= limit — без переполнения */
            u64 start = (my_lo + p - 1) / p * p;
            if (start < pp) start = pp;
            for (u64 j = start; j < my_hi; j += p) flags[j - my_lo] = 0;
        }
        for (u64 j = 0; j < my_len; j++) if (flags[j]) my_count++;
    }

    /* --- 4. узнаём, сколько у кого простых --- */
    u64 *counts = NULL;
    int *rc = NULL, *dp = NULL;
    if (rank == 0) {
        counts = malloc((size_t)size * sizeof(u64));
        rc     = malloc((size_t)size * sizeof(int));
        dp     = malloc((size_t)size * sizeof(int));
        if (!counts || !rc || !dp) {
            free(small); free(flags); free(counts); free(rc); free(dp);
            return PAR_MEM_ERR;
        }
    }
    MPI_Gather(&my_count, 1, MPI_UINT64_T,
               counts, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    u64 total_primes = ns;
    if (rank == 0) {
        u64 acc = 0;
        for (int i = 0; i < size; i++) {
            rc[i] = (int)(counts[i] * sizeof(u64));
            dp[i] = (int)acc;
            acc += counts[i] * sizeof(u64);
            total_primes += counts[i];
        }
    }
    if (rank != 0) {
        rc = malloc((size_t)size * sizeof(int));
        dp = malloc((size_t)size * sizeof(int));
        if (!rc || !dp) {
            free(small); free(flags); free(counts); free(rc); free(dp);
            return PAR_MEM_ERR;
        }
    }
    MPI_Bcast(&total_primes, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(rc, size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(dp, size, MPI_INT, 0, MPI_COMM_WORLD);

    /* --- 5. упаковка локального блока в массив --- */
    u64 *lbuf = NULL;
    if (my_count > 0) {
        lbuf = malloc((size_t)my_count * sizeof(u64));
        if (!lbuf) {
            free(small); free(flags); free(counts); free(rc); free(dp);
            return PAR_MEM_ERR;
        }
        u64 idx = 0;
        for (u64 j = 0; j < my_len; j++)
            if (flags[j]) lbuf[idx++] = my_lo + j;
    }
    free(flags);

    /* --- 6. Allgatherv: полный список простых на каждом ранге --- */
    u64 *all = malloc((size_t)total_primes * sizeof(u64));
    if (!all) {
        free(small); free(lbuf); free(counts); free(rc); free(dp);
        return PAR_MEM_ERR;
    }
    if (ns > 0) memcpy(all, small, (size_t)ns * sizeof(u64));

    void *sb = lbuf ? (void *)lbuf : (void *)&my_count;
    MPI_Allgatherv(sb, (int)(my_count * sizeof(u64)), MPI_BYTE,
                   all + ns, rc, dp, MPI_BYTE, MPI_COMM_WORLD);

    free(small); free(lbuf); free(counts); free(rc); free(dp);

    *primes_out = all;
    *count_out  = total_primes;
    return NO_ERR;
}

/* ------------------------------------------------------------------ */
/*  ПАРАЛЛЕЛЬНАЯ СОРТИРОВКА — SAMPLE SORT                             */
/*                                                                    */
/*  После вызова: rank r содержит r-й кусок глобально отсортированного */
/*  массива (rank 0 — самые маленькие, rank size-1 — самые большие).   */
/* ------------------------------------------------------------------ */

static lab_err_t parallel_sort(int size, result_t *res)
{
    lab_err_t err = NO_ERR;
    const u64 p        = (u64)size;
    const u64 n_split  = p - 1;

    u64 *local_split = NULL;
    u64 *all_split   = NULL;
    u64 *g_split     = NULL;
    u64 *bin_send    = NULL;
    u64 *bin_recv    = NULL;
    int *sc = NULL, *sd = NULL, *rc = NULL, *rd = NULL;
    triple_entry_t *sbuf = NULL, *rbuf = NULL;
    u64 *off = NULL;
    const triple_entry_t **runs = NULL;
    u64 *rsz = NULL, *ridx = NULL;

    err = result_sort(res);
    if (err != NO_ERR) return err;
    if (size <= 1)     return NO_ERR;

    const u64 n_local = res->count;

    /* 2. p-1 локальных сплиттеров */
    if (n_split > 0) {
        local_split = malloc((size_t)n_split * sizeof(u64));
        if (!local_split) { err = PAR_MEM_ERR; goto end; }
        for (u64 i = 0; i < n_split; i++) {
            if (n_local == 0) {
                local_split[i] = UINT64_MAX;
            } else {
                u64 idx = (n_local * (i + 1)) / p;
                if (idx >= n_local) idx = n_local - 1;
                local_split[i] = res->entries[idx].value;
            }
        }
    }

    /* 3. Allgather сплиттеров */
    u64 total_split = p * n_split;
    if (total_split > 0) {
        all_split = malloc((size_t)total_split * sizeof(u64));
        if (!all_split) { err = PAR_MEM_ERR; goto end; }
        int nb = (int)(n_split * sizeof(u64));
        MPI_Allgather(local_split, nb, MPI_BYTE,
                      all_split, nb, MPI_BYTE, MPI_COMM_WORLD);
    }

    /* 4. глобальные p-1 сплиттеров */
    if (n_split > 0) {
        qsort(all_split, (size_t)total_split, sizeof(u64), cmp_u64_asc);
        g_split = malloc((size_t)n_split * sizeof(u64));
        if (!g_split) { err = PAR_MEM_ERR; goto end; }
        for (u64 i = 0; i < n_split; i++) {
            u64 idx = (total_split * (i + 1)) / p;
            if (idx >= total_split) idx = total_split - 1;
            g_split[i] = all_split[idx];
        }
    }

    /* 5. размеры корзин */
    bin_send = calloc((size_t)p, sizeof(u64));
    bin_recv = malloc((size_t)p * sizeof(u64));
    if (!bin_send || !bin_recv) { err = PAR_MEM_ERR; goto end; }

    for (u64 i = 0; i < n_local; i++) {
        u64 b = upper_bound_u64(g_split, n_split, res->entries[i].value);
        bin_send[b]++;
    }
    MPI_Alltoall(bin_send, 1, MPI_UINT64_T,
                 bin_recv, 1, MPI_UINT64_T, MPI_COMM_WORLD);

    /* 6. байтовые counts/displs */
    sc = malloc((size_t)p * sizeof(int));
    sd = malloc((size_t)p * sizeof(int));
    rc = malloc((size_t)p * sizeof(int));
    rd = malloc((size_t)p * sizeof(int));
    if (!sc || !sd || !rc || !rd) { err = PAR_MEM_ERR; goto end; }

    u64 tot_send = 0, tot_recv = 0;
    for (u64 i = 0; i < p; i++) {
        sc[i] = (int)(bin_send[i] * sizeof(triple_entry_t));
        sd[i] = (int)tot_send;
        tot_send += bin_send[i] * sizeof(triple_entry_t);
        rc[i] = (int)(bin_recv[i] * sizeof(triple_entry_t));
        rd[i] = (int)tot_recv;
        tot_recv += bin_recv[i] * sizeof(triple_entry_t);
    }

    /* 7. упаковка в sbuf — С ПРАВИЛЬНЫМ смещением */
    off = calloc((size_t)p, sizeof(u64));
    if (!off) { err = PAR_MEM_ERR; goto end; }
    if (tot_send > 0) {
        sbuf = malloc(tot_send);
        if (!sbuf) { err = PAR_MEM_ERR; goto end; }
    }
    for (u64 i = 0; i < n_local; i++) {
        u64 b = upper_bound_u64(g_split, n_split, res->entries[i].value);
        u64 boff = sd[b] + off[b] * sizeof(triple_entry_t);   /* ← ФИКС */
        off[b]++;
        memcpy((char *)sbuf + boff, &res->entries[i], sizeof(triple_entry_t));
    }

    /* 8. Alltoallv */
    if (tot_recv > 0) {
        rbuf = malloc(tot_recv);
        if (!rbuf) { err = PAR_MEM_ERR; goto end; }
    }
    void *sb = sbuf ? (void *)sbuf : (void *)&tot_send;
    void *rb = rbuf ? (void *)rbuf : (void *)&tot_recv;
    MPI_Alltoallv(sb, sc, sd, MPI_BYTE,
                  rb, rc, rd, MPI_BYTE, MPI_COMM_WORLD);

    /* 9. k-way merge p отсортированных кусков */
    u64 n_total = tot_recv / sizeof(triple_entry_t);

    free(res->entries);
    res->entries  = (n_total > 0)
                  ? malloc((size_t)n_total * sizeof(triple_entry_t))
                  : NULL;
    res->count    = 0;
    res->capacity = n_total;
    if (n_total > 0 && !res->entries) { err = PAR_MEM_ERR; goto end; }

    runs = malloc((size_t)p * sizeof(*runs));
    rsz  = malloc((size_t)p * sizeof(u64));
    ridx = malloc((size_t)p * sizeof(u64));
    if (!runs || !rsz || !ridx) { err = PAR_MEM_ERR; goto end; }

    for (u64 i = 0; i < p; i++) {
        runs[i] = (const triple_entry_t *)((const char *)rbuf + rd[i]);
        rsz[i]  = bin_recv[i];
        ridx[i] = 0;
    }
    for (u64 k = 0; k < n_total; k++) {
        int best = -1;
        for (int i = 0; i < size; i++) {
            if (ridx[i] >= rsz[i]) continue;
            if (best < 0 ||
                cmp_triple(&runs[i][ridx[i]], &runs[best][ridx[best]]) < 0)
            {
                best = i;
            }
        }
        if (best < 0) break;
        res->entries[res->count++] = runs[best][ridx[best]++];
    }

end:
    free(local_split);
    free(all_split);
    free(g_split);
    free(bin_send);
    free(bin_recv);
    free(sc); free(sd); free(rc); free(rd);
    free(sbuf); free(rbuf);
    free(off);
    free(runs); free(rsz); free(ridx);
    return err;
}

/* ------------------------------------------------------------------ */
/*  СБОР НА RANK 0                                                    */
/*                                                                    */
/*  После parallel_sort rank r держит r-й кусок глобально сортированного */
/*  массива; ранг 0 — самые маленькие. Просто конкатенируем по рангам. */
/* ------------------------------------------------------------------ */

static lab_err_t gather_to_root(int rank, int size, result_t *res)
{
    u64 local = res->count;
    u64 *counts = NULL;
    int *rc = NULL, *dp = NULL;
    triple_entry_t *all = NULL;
    u64 total = 0;

    if (rank == 0) {
        counts = malloc((size_t)size * sizeof(u64));
        rc     = malloc((size_t)size * sizeof(int));
        dp     = malloc((size_t)size * sizeof(int));
        if (!counts || !rc || !dp) {
            free(counts); free(rc); free(dp);
            return PAR_MEM_ERR;
        }
    }
    MPI_Gather(&local, 1, MPI_UINT64_T,
               counts, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        u64 acc = 0;
        for (int i = 0; i < size; i++) {
            rc[i] = (int)(counts[i] * sizeof(triple_entry_t));
            dp[i] = (int)acc;
            acc += counts[i] * sizeof(triple_entry_t);
            total += counts[i];
        }
        if (total > 0) {
            all = malloc((size_t)total * sizeof(triple_entry_t));
            if (!all) {
                free(counts); free(rc); free(dp);
                return PAR_MEM_ERR;
            }
        }
    }

    void *sb = res->entries ? (void *)res->entries : (void *)&local;
    void *rb = all ? (void *)all : (void *)&total;
    MPI_Gatherv(sb, (int)(local * sizeof(triple_entry_t)), MPI_BYTE,
                rb, rc, dp, MPI_BYTE, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        free(res->entries);
        res->entries  = all;
        res->count    = total;
        res->capacity = total;
    } else {
        result_free(res);            /* на прочих рангах результат не нужен */
    }

    free(counts); free(rc); free(dp);
    return NO_ERR;
}

/* ------------------------------------------------------------------ */
/*  ГЛАВНАЯ ПАРАЛЛЕЛЬНАЯ ФУНКЦИЯ                                      */
/* ------------------------------------------------------------------ */

lab_err_t parallel(u64 m, u64 n, result_t *res)
{
    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);   /* динамическое обнаружение ресурсов */

    lab_err_t err = NO_ERR;
    u64 *primes = NULL, np = 0;

    result_init(res);
    clock_gettime(CLOCK_MONOTONIC, &par_start);

    /* 1. ПАРАЛЛЕЛЬНОЕ решето — блоки (√limit, limit] */
    err = parallel_sieve(n / 6 + 10, rank, size, &primes, &np);
    if (err != NO_ERR) goto end;

    /* 2. Перебор троек — round-robin по индексам p1 */
    err = enumerate_triples(m, n, primes, np, (u64)rank, (u64)size, res);
    if (err != NO_ERR) goto end;

    /* 3. ПАРАЛЛЕЛЬНАЯ сортировка (sample sort) */
    err = parallel_sort(size, res);
    if (err != NO_ERR) goto end;

    /* 4. Сбор на rank 0 (просто конкатенация по рангам) */
    err = gather_to_root(rank, size, res);

end:
    free(primes);
    clock_gettime(CLOCK_MONOTONIC, &par_end);
    return err;
}