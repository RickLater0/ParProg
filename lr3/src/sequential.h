#ifndef SEQUENTIAL_H__
#define SEQUENTIAL_H__

#include "common.h"
#include <time.h>

lab_err_t sequential(
    u64 m,
    u64 n,
    result_t *res
);

extern solution s_solution;
extern struct timespec seq_start;
extern struct timespec seq_end;

#endif /* SEQUENTIAL_H__ */