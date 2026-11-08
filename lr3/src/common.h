#ifndef COMMON_H__
#define COMMON_H__

#include <stdint.h>
#include <stdlib.h>

#define u8  uint8_t
#define u32 uint32_t
#define u64 uint64_t

#define MODULE_NAME "[LAB_3]"

typedef enum lab_err {
    NO_ERR = 0,

    ARG_ERR            = -1,   /* некорректные аргументы / NULL              */
    MEM_ERR            = -2,   /* недостаточно памяти                        */

    PAR_NO_DEVICE      = -5,   /* MPI-среда не инициализирована              */
    PAR_PROG_BUILD_ERR = -6,   /* ошибка сборки/подготовки параллельной части*/
    PAR_INIT_ERR       = -7,   /* ошибка инициализации MPI                   */
    PAR_MEM_ERR        = -8,   /* ошибка выделения MPI-буферов               */
    PAR_ARG_ERR        = -9,   /* ошибка аргументов параллельной части       */
    PAR_EXEC_ERR       = -10,  /* ошибка выполнения параллельной части       */
} lab_err_t;

/* одна найденная тройка: value = p1*p2*p3, p1 < p2 < p3 — простые */
typedef struct {
    u64 value;
    u64 p1;
    u64 p2;
    u64 p3;
} triple_entry_t;

typedef struct {
    triple_entry_t *entries;
    u64             count;
    u64             capacity;
} result_t;

typedef lab_err_t (*solution)(
    u64 m,
    u64 n,
    result_t *res
);

lab_err_t result_init  (result_t *r);
lab_err_t result_append(result_t *r, triple_entry_t e);
lab_err_t result_sort  (result_t *r);
void      result_free  (result_t *r);

lab_err_t sieve(
    const u64 N,
    u64 **primes,
    size_t *p_sz
);

lab_err_t enumerate_triples(
    u64 m, u64 n,
    u64 *primes, u64 np,
    u64 rank, u64 size,
    result_t *res
);

const char *lab_err_string(lab_err_t lerr);

#endif /* COMMON_H__ */