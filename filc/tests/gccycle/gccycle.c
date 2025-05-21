#include <stdfil.h>
#include "utils.h"

int main()
{
    zgc_wait(0);
    zgc_wait(zgc_try_request());
    zgc_wait(zgc_request_fresh());
    zgc_wait(zgc_completed_cycle());
    zgc_wait(zgc_requested_cycle());
    
    unsigned count;
    for (count = 1000000; count--;) {
        opaque(zgc_alloc(1));
        zgc_cycle_number completed = zgc_completed_cycle();
        zfence();
        zgc_cycle_number requested = zgc_requested_cycle();
        ZASSERT(requested >= completed);
    }
    zprintf("requested cycle = %llu, completed cycle = %llu\n",
            (unsigned long long)zgc_requested_cycle(), (unsigned long long)zgc_completed_cycle());

    zgc_wait(0);
    zgc_wait(zgc_completed_cycle());
    zgc_wait(zgc_requested_cycle());
    zgc_wait(zgc_try_request());
    zgc_wait(zgc_request_fresh());

    return 0;
}

