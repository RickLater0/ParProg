#ifndef COMMON_H__
#define COMMON_H__

#include <stdint.h>

#define u8 uint8_t
#define u32 uint32_t
#define u64 uint64_t

#define MODULE_NAME "[LAB_2]"

typedef enum lab_err{
    NO_ERR = 0,

    ARG_ERR              = -1, //некоторые аргументы равны NULL
    MEM_ERR              = -2, //недостаточно памяти для выделения ресурсов
    
    PAR_NO_DEVICE        = -5, //подходящее OpenCL устройство (GPU/CPU) не найдено
    PAR_PROG_BUILD_ERR   = -6, //ошибка сборки OpenCL программы (kernel source)
    PAR_INIT_ERR         = -7, //ошибка инициализации OpenCL контекста или очереди команд
    PAR_MEM_ERR          = -8, //ошибка выделения буфера в памяти OpenCL устройства
    PAR_ARG_ERR          = -9, //ошибка установки аргументов OpenCL ядра
    PAR_EXEC_ERR         = -10,//ошибка выполнения OpenCL ядра (EnqueueNDRangeKernel)
} lab_err_t;

typedef struct {
    u8 n;
    u8 **matrix;
} graph_t;

typedef lab_err_t ( *solution) (
    graph_t *g1,
    graph_t *g2,
    u8 *res
);

lab_err_t graph_init(
    graph_t *graph,
    u8 n,
    u8 **matrix
);
void graph_free(graph_t *graph);

const char *lab_err_string(const lab_err_t lerr);

#endif //COMMON_H__