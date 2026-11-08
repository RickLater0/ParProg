#ifndef PARALLEL_H__
#define PARALLEL_H__

#include "common.h"
#include <time.h>

lab_err_t parallel(u64 m, u64 n, result_t *res);

extern solution p_solution;
extern struct timespec par_start;
extern struct timespec par_end;

#endif /* PARALLEL_H__ */