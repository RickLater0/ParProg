#ifndef PARALLEL_H__
#define PARALLEL_H__

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

lab_err_t parallel(
    const params_t *params,
    f64 *top_face
);

extern solution p_solution;
extern struct timespec par_start;
extern struct timespec par_end;

#ifdef __cplusplus
}
#endif

#endif /* PARALLEL_H__ */