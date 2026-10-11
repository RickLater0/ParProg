#ifndef PARALLEL_H
#define PARALLEL_H

#include "common.h"

/**
 * @brief Найти наибольшее число M, меньшее заданного числа N, 
 * которое можно представить суммой k различных простых чисел.
 * 
 * @param[in] N входное число
 * @param[in] k количество простых чисел
 * @param[out] M массив суммируемых простых чисел
 * @return seq_err_t код ошибки
 */
lab_err_t parallel(
    const u64 N,
    const size_t k,
    u64 **M
);

#endif