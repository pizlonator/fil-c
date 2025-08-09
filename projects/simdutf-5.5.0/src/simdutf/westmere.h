#ifndef SIMDUTF_WESTMERE_H
#define SIMDUTF_WESTMERE_H

#ifdef SIMDUTF_FALLBACK_H
#error "westmere.h must be included before fallback.h"
#endif

#include "simdutf/portability.h"

// Default Westmere to on if this is x86-64, unless we'll always select Haswell.
#ifndef SIMDUTF_IMPLEMENTATION_WESTMERE
//
// You do not want to set it to (SIMDUTF_IS_X86_64 && !SIMDUTF_REQUIRES_HASWELL)
// because you want to rely on runtime dispatch!
//
#if SIMDUTF_CAN_ALWAYS_RUN_ICELAKE || SIMDUTF_CAN_ALWAYS_RUN_HASWELL
#define SIMDUTF_IMPLEMENTATION_WESTMERE 0
#else
#define SIMDUTF_IMPLEMENTATION_WESTMERE (SIMDUTF_IS_X86_64)
#endif

#endif

#if (SIMDUTF_IMPLEMENTATION_WESTMERE && SIMDUTF_IS_X86_64 && __SSE4_2__)
#define SIMDUTF_CAN_ALWAYS_RUN_WESTMERE 1
#else
#define SIMDUTF_CAN_ALWAYS_RUN_WESTMERE 0
#endif

#if SIMDUTF_IMPLEMENTATION_WESTMERE

#define SIMDUTF_TARGET_WESTMERE SIMDUTF_TARGET_REGION("sse4.2,popcnt")

namespace simdutf {
/**
 * Implementation for Westmere (Intel SSE4.2).
 */
namespace westmere {
} // namespace westmere
} // namespace simdutf

//
// These two need to be included outside SIMDUTF_TARGET_REGION
//
#include "simdutf/westmere/implementation.h"
#include "simdutf/westmere/intrinsics.h"

//
// The rest need to be inside the region
//
#include "simdutf/westmere/begin.h"

// Declarations
#include "simdutf/westmere/bitmanipulation.h"
#include "simdutf/westmere/simd.h"

#include "simdutf/westmere/end.h"

#endif // SIMDUTF_IMPLEMENTATION_WESTMERE
#endif // SIMDUTF_WESTMERE_COMMON_H
