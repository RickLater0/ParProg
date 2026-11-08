#ifndef PARALLEL_H__
#define PARALLEL_H__

#include "common.h"
#include <time.h>

lab_err_t parallel(
    u64 m,
    u64 n,
    result_t *res
);

lab_err_t parallel_init(
    int *argc,
    char ***argv
);

lab_err_t parallel_finalize(void);

int parallel_rank(void);

int parallel_size(void);

lab_err_t parallel_broadcast_input(
    u64 *m,
    u64 *n,
    int *ok
);

lab_err_t parallel_sync_err(
    lab_err_t err
);

extern solution p_solution;
extern struct timespec par_start;
extern struct timespec par_end;

#endif /* PARALLEL_H__ */