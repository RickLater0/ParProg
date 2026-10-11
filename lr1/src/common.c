#include "common.h"

lab_err_t sieve(
    const u64 N,
    u64 **primes,
    size_t *p_sz
)
{
    u8 *primes_bool = NULL;
    size_t primes_len = 0;
    u64 *res = NULL;
    size_t ind = 0;

    if(!primes || !p_sz || N < 2)
    {
        return ARG_ERR;
    }

    primes_bool = calloc((size_t)(N + 1), sizeof(u8));
    if(!primes_bool)
    {
        return MEM_ERR;
    }

    for(u64 i = 0; i <= N; i++)
    {
        primes_bool[i] = 1;
    }
    primes_bool[0] = 0;
    primes_bool[1] = 0;

    for(u64 p = 2; p * p <= N; p++)
    {
        if(primes_bool[p] == 0)
        {
            continue;
        }
        for(u64 i = p * p; i <= N; i += p)
        {
            primes_bool[i] = 0;
        }
    }

    for(u64 i = 0; i <= N; i++){
        if(primes_bool[i] != 0)
        {
            primes_len++;
        }
    }

    res = calloc(primes_len, sizeof(u64));
    if(!res)
    {
        return MEM_ERR;
    }
    
    ind = 0;
    for(u64 i = 0; i <= N; i++){
        if(primes_bool[i] != 0)
        {
            res[ind] = i;
            ind++;
        }
    }

    free(primes_bool);
    *p_sz = primes_len;
    *primes = res;
    return NO_ERR;
}

char *lab_err_string(lab_err_t err)
{
    switch (err) 
    {
        case NO_ERR:
            return "No error";
        case ARG_ERR:
            return "Some args are NULL";
        case MEM_ERR:
            return "Not enough memory";
        case LARGE_K:
            return "Can't provide sum for this N because of big \'k\' value";
        case NO_SUM:
            return "No sum find";
        
        case PAR_NO_DEVICE:
            return "OpenCL device not found";
        case PAR_PROG_BUILD_ERR:
            return "OpenCL program build failed";
        case PAR_INIT_ERR:
            return "OpenCL context or queue initialization failed";
        case PAR_MEM_ERR:
            return "OpenCL buffer allocation failed";
        case PAR_ARG_ERR:
            return "OpenCL kernel argument setting failed";
        case PAR_EXEC_ERR:
            return "OpenCL kernel execution failed";
        
        default:
            return "[LR_1] Unknown error";
    }
}