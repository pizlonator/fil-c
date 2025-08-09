#ifndef SIMDUTF_ARM64_H
#define SIMDUTF_ARM64_H

#ifdef SIMDUTF_FALLBACK_H
#error "arm64.h must be included before fallback.h"
#endif

#include "simdutf/portability.h"

#ifndef SIMDUTF_IMPLEMENTATION_ARM64
#define SIMDUTF_IMPLEMENTATION_ARM64 (SIMDUTF_IS_ARM64)
#endif
#if SIMDUTF_IMPLEMENTATION_ARM64 && SIMDUTF_IS_ARM64
#define SIMDUTF_CAN_ALWAYS_RUN_ARM64 1
#else
#define SIMDUTF_CAN_ALWAYS_RUN_ARM64 0
#endif


#include "simdutf/internal/isadetection.h"

#if SIMDUTF_IMPLEMENTATION_ARM64

namespace simdutf {
/**
 * Implementation for NEON (ARMv8).
 */
namespace arm64 {
} // namespace arm64
} // namespace simdutf

#include "simdutf/arm64/implementation.h"

#include "simdutf/arm64/begin.h"

// Declarations
#include "simdutf/arm64/intrinsics.h"
#include "simdutf/arm64/bitmanipulation.h"
#include "simdutf/arm64/simd.h"

#include "simdutf/arm64/end.h"

#endif // SIMDUTF_IMPLEMENTATION_ARM64

#endif // SIMDUTF_ARM64_H
