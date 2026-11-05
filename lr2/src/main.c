#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#define HELP_MESSAGE "Usage: %s <n> <matrix1> <matrix2> \n\
            where\n\
            n - matrix size\n\
            matrix - array n*n elements with 0 (nolink) or 1 (linked)\n"

static lab_err_t parse_matrix(
    u8 n,
    char **str,
    u8 ***matrix1,
    u8 ***matrix2
);

static lab_err_t test(
    solution sol,
    graph_t *g1,
    graph_t *g2,
    u8 *res
);

static void print_out(
    graph_t* graph,
    u8 res
);

static u8 print_diff(
    u8 res,
    graph_t *g1,
    graph_t *g2
);

int main(int argc, char **argv)
{
    u8 n = 0;
    u8 **matrix1 = NULL;
    u8 **matrix2 = NULL;
    u8 res = 0;

    graph_t graph1 = {};
    graph_t graph2 = {};

    if(argc == 1)
    {
        printf(HELP_MESSAGE, argv[0]);
        return 0;
    }
    n = (u8)strtoul(argv[1], NULL, 10);
    if(argc != 2 * n * n + 2) //2 * n * n - две матрицы, 2 это сам n и argv[0]
    {
        printf(HELP_MESSAGE, argv[0]);
        return 0;
    }

    parse_matrix(n, argv + 2, &matrix1, &matrix1);
    graph_init(&graph1, n, matrix1);
    graph_init(&graph2, n, matrix2);

    

}
