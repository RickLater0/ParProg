#ifndef SEQUENTIAL_H__
#define SEQUENTIAL_H__

#include "common.h"

lab_err_t sequential(
    const graph_t *g1,
    const graph_t *g2,
    graph_t *ret_g1,
    graph_t *ret_g2,
    u8 *res
);

extern solution s_solution;
extern struct timespec seq_start;
extern struct timespec seq_end;

#endif //SEQUENTIAL_H__