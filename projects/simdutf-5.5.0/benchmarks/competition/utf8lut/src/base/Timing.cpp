#include "base/Timing.h"

#ifdef TIMING

TimingData timingData = TimingData();
void TimingPrintAll(FILE *f) {
    #define TIMING_X_PRINT(name, idx) \
        if (idx == TIMING_MAX) return; \
        fprintf(f, "slot %d %10s : %6.3f cyc/el  %12" PRIu64 " elems\n", \
            idx, #name, \
            timingData.totalTime[idx] / (timingData.totalElems[idx] + 1e-9), \
            timingData.totalElems[idx] \
        );
    TIMING_SLOTS(TIMING_X_PRINT);
}

#endif
