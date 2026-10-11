#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>

#define u64 uint64_t
#define u8  uint8_t

typedef enum lab_err{
    NO_ERR = 0,

    ARG_ERR              = -1, //некоторые аргументы равны NULL
    MEM_ERR              = -2, //недостаточно памяти для выделения ресурсов
    NO_SUM               = -3, //подходящая сумма не найдена. Возможно, N слишком мало?
    LARGE_K              = -4, //невозможно найти сумму из-за слишком большого значения 'k'
    
    PAR_NO_DEVICE        = -5, //подходящее OpenCL устройство (GPU/CPU) не найдено
    PAR_PROG_BUILD_ERR   = -6, //ошибка сборки OpenCL программы (kernel source)
    PAR_INIT_ERR         = -7, //ошибка инициализации OpenCL контекста или очереди команд
    PAR_MEM_ERR          = -8, //ошибка выделения буфера в памяти OpenCL устройства
    PAR_ARG_ERR          = -9, //ошибка установки аргументов OpenCL ядра
    PAR_EXEC_ERR         = -10,//ошибка выполнения OpenCL ядра (EnqueueNDRangeKernel)
} lab_err_t;

/**
 * @brief решето Эратосфена
 * 
 * @param[in]  N верхняя граница
 * @param[out] primes массив простых чисел
 * @param[out] p_sz размер выходного массива
 * @return seq_err_t код ошибки
 */
lab_err_t sieve(
    const uint64_t N,
    uint64_t **primes,
    size_t *p_sz
);

/**
 * @brief преобразует код ошибки в строку
 * 
 * @param err код ошибки
 * @return char* строка с описанием ошибки
 */
char *lab_err_string(lab_err_t err);

#endif