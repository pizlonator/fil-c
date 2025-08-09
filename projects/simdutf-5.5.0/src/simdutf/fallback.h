#ifndef SIMDUTF_FALLBACK_H
#define SIMDUTF_FALLBACK_H

#include "simdutf/portability.h"

// Note that fallback.h is always imported last.

// Default Fallback to on unless a builtin implementation has already been selected.
#ifndef SIMDUTF_IMPLEMENTATION_FALLBACK
#if SIMDUTF_CAN_ALWAYS_RUN_ARM64 || SIMDUTF_CAN_ALWAYS_RUN_ICELAKE || SIMDUTF_CAN_ALWAYS_RUN_HASWELL || SIMDUTF_CAN_ALWAYS_RUN_WESTMERE || SIMDUTF_CAN_ALWAYS_RUN_PPC64 || SIMDUTF_CAN_ALWAYS_RUN_RVV
#define SIMDUTF_IMPLEMENTATION_FALLBACK 0
#else
#define SIMDUTF_IMPLEMENTATION_FALLBACK 1
#endif
#endif

#define SIMDUTF_CAN_ALWAYS_RUN_FALLBACK (SIMDUTF_IMPLEMENTATION_FALLBACK)

#if SIMDUTF_IMPLEMENTATION_FALLBACK

namespace simdutf {
/**
 * Fallback implementation (runs on any machine).
 */
namespace fallback {
} // namespace fallback
} // namespace simdutf

#include "simdutf/fallback/implementation.h"

#include "simdutf/fallback/begin.h"

// Declarations
#include "simdutf/fallback/bitmanipulation.h"

#include "simdutf/fallback/end.h"

#endif // SIMDUTF_IMPLEMENTATION_FALLBACK
#endif // SIMDUTF_FALLBACK_H
