#pragma once

#ifdef TIMING
//define all this stuff only if asked for

#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#ifdef _MSC_VER
    #define PRIu64 "I64u"
    #define PRId64 "I64d"
#else
    #define __STDC_FORMAT_MACROS 1
    #include <inttypes.h>
#endif

#ifdef _MSC_VER
    #include <intrin.h>
#else
    #include <x86intrin.h>
#endif
static inline uint64_t get_ticks() {
    return __rdtsc();
}

#define TIMING_SLOTS(X) \
    X(DECODE, 1) \
    X(ENCODE, 2) \
    X(MAX, 3)

//======================================================

#define CONCAT(x, y) x##y

#define TIMING_X_DEFINE(name, idx) \
    static const int CONCAT(TIMING_, name) = idx;
TIMING_SLOTS(TIMING_X_DEFINE);

struct TimingData {
    uint64_t totalTime[TIMING_MAX];
    uint64_t totalElems[TIMING_MAX];
    uint64_t startTime[TIMING_MAX];
};

extern TimingData timingData;

//======================================================

#define TIMING_START(name) { \
    int slot = CONCAT(TIMING_, name); \
    uint64_t &startTime = timingData.startTime[slot]; \
    startTime = get_ticks(); \
}

#define TIMING_END(name, elems) { \
    uint64_t endTime = get_ticks(); \
    int slot = CONCAT(TIMING_, name); \
    uint64_t &startTime = timingData.startTime[slot]; \
    assert(startTime != 0); \
    timingData.totalTime[slot] += endTime - startTime; \
    timingData.totalElems[slot] += uint64_t(elems); \
    startTime = 0; \
}

void TimingPrintAll(FILE *f);
#define TIMING_PRINT(f) TimingPrintAll(f);

#else
//if not asked (TIMING undefined), then define empty macros

#define TIMING_START(name)
#define TIMING_END(name, elems)
#define TIMING_PRINT(f)

#endif
