#define SIMDUTF_IMPLEMENTATION rvv

#if SIMDUTF_CAN_ALWAYS_RUN_RVV
// nothing needed.
#else
SIMDUTF_TARGET_RVV
#endif
