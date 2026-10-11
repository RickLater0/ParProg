#define u64 unsigned long
#define u8 unsigned char

__kernel void check_candidates(
    __global const u64 *primes,
    const int p_sz,
    const int k,
    __global const u64 *candidates,
    const int cand_count,
    __global int *results,
    __global u64 *out_combinations
) {
    int gid = get_global_id(0);
    if (gid >= cand_count) 
    {
        return;
    }

    u64 target = candidates[gid];
    int found = 0;

    if (k == 1) 
    {
        int low = 0, high = p_sz - 1;
        while (low <= high) 
        {
            int mid = low + (high - low) / 2;
            if (primes[mid] == target) 
            {
                found = 1;
                out_combinations[gid * k] = primes[mid];
                break;
            }
            else if (primes[mid] < target) 
            {
                low = mid + 1;
            }
            else 
            {
                high = mid - 1;
            }
        }
        results[gid] = found;
        return;
    }

    int iter_stack_idx[16];
    int iter_prime_idx[16];
    for(int i = 0; i < 16; i++) 
    {
        iter_stack_idx[i] = 0;
        iter_prime_idx[i] = 0;
    }

    int d = 0;
    iter_prime_idx[0] = 0;
    u64 c_sum = 0;

    while (1) 
    {
        if (d == (int)k) 
        {
            if (c_sum == target) 
            {
                found = 1;
                for (int i = 0; i < k; i++) 
                {
                    out_combinations[gid * (int)k + i] = primes[iter_stack_idx[i]];
                }
                break;
            }
            d--;
            if (d < 0) 
            { 
                break;
            }
            c_sum -= primes[iter_stack_idx[d]];
            iter_prime_idx[d]++;
        } 
        else 
        {
            int start = iter_prime_idx[d];
            if (
                p_sz - start < (int)k - d || 
                (c_sum + primes[start] > target && primes[start] > 0)
            ) 
            {
                d--;
                if (d < 0) 
                {
                    break;
                }
                c_sum -= primes[iter_stack_idx[d]];
                iter_prime_idx[d]++;
                continue;
            }
            if (start < p_sz) 
            {
                u64 p_val = primes[start];
                if (c_sum + p_val > target) 
                {
                    d--;
                    if (d < 0) break;
                    c_sum -= primes[iter_stack_idx[d]];
                    iter_prime_idx[d]++;
                } 
                else 
                {
                    iter_stack_idx[d] = start;
                    c_sum += p_val;
                    d++;
                    if (d < (int)k) 
                    {
                        iter_prime_idx[d] = start + 1;
                    }
                }
            } else {
                d--;
                if (d < 0)
                {
                    break;
                }
                c_sum -= primes[iter_stack_idx[d]];
                iter_prime_idx[d]++;
            }
        }
    }
    results[gid] = found;
}