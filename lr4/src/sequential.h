#ifndef SEQUENTIAL_H__
#define SEQUENTIAL_H__

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

lab_err_t sequential(
    const params_t *params,
    f64 *top_face
);

extern solution s_solution;
extern struct timespec seq_start;
extern struct timespec seq_end;

#ifdef __cplusplus
}
#endif

#endif /* SEQUENTIAL_H__ */