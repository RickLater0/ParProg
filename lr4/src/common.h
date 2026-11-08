#ifndef COMMON_H__
#define COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define u8  uint8_t
#define u32 uint32_t
#define u64 uint64_t
#define f64 double

#define MODULE_NAME "[LAB_4]"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Lab_err
{
    NO_ERR         = 0,

    ARG_ERR        = -1,   // некорректные аргументы / NULL
    MEM_ERR        = -2,   // недостаточно памяти

    CUDA_NO_DEVICE = -5,   // CUDA-устройство не найдено
    CUDA_ERR       = -6,   // ошибка CUDA
} lab_err_t;

typedef struct
{
    u64 k;
    u64 m;
    u64 n;
    f64 t1;
    f64 t2;
    u64 radius;
    u64 max_iter;
} params_t;

typedef struct
{
    u64 dim_x;
    u64 dim_y;
    u64 dim_z;
    f64 *data;
} grid_t;

typedef lab_err_t (*solution)(
    const params_t *params,
    f64 *top_face
);

/**
 * @brief Инициализирует 3D-сетку заданных размеров.
 * @param g     Указатель на структуру сетки.
 * @param dim_x Размер по X.
 * @param dim_y Размер по Y.
 * @param dim_z Размер по Z.
 * @return NO_ERR при успехе, иначе код ошибки.
 */
lab_err_t grid_init(
    grid_t *g,
    u64 dim_x,
    u64 dim_y,
    u64 dim_z
);

/**
 * @brief Освобождает память, занятую сеткой.
 * @param g Указатель на структуру сетки.
 */
void grid_free(
    grid_t *g
);

/**
 * @brief Заполняет граничные условия, начальное приближение и маску полости.
 * @param g    Сетка (data уже выделена).
 * @param mask Маска: 0 — полость, 1 — внутренняя точка, 2 — фиксированная граница.
 * @param p    Параметры задачи.
 * @return NO_ERR при успехе, иначе код ошибки.
 */
lab_err_t grid_setup(
    grid_t *g,
    u8 *mask,
    const params_t *p
);

/**
 * @brief Копирует верхнюю грань (z = dim_z - 1) в плоский массив.
 * @param g        Сетка.
 * @param top_face Выходной массив размера dim_x * dim_y.
 */
void grid_extract_top(
    const grid_t *g,
    f64 *top_face
);

/**
 * @brief Возвращает текстовое описание кода ошибки.
 * @param err Код ошибки.
 * @return Строка с описанием.
 */
const char *lab_err_string(
    lab_err_t err
);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H__ */