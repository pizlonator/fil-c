#include <stdfil.h>
#include "utils.h"

int main()
{
    zweak* weak = zweak_new(zgc_alloc(1));
    zgc_cycle_number cycle = zgc_completed_cycle();
    while (zgc_completed_cycle() < cycle + 2) {
        zgc_cycle_number current_cycle = zgc_completed_cycle();
        ZASSERT(zgc_try_request() > current_cycle);
    }
    ZASSERT(!zweak_get(weak));
    return 0;
}


