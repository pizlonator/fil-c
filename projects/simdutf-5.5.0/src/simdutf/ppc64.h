#ifndef SIMDUTF_PPC64_H
#define SIMDUTF_PPC64_H

#ifdef SIMDUTF_FALLBACK_H
#error "ppc64.h must be included before fallback.h"
#endif

#include "simdutf/portability.h"

#ifndef SIMDUTF_IMPLEMENTATION_PPC64
#define SIMDUTF_IMPLEMENTATION_PPC64 (SIMDUTF_IS_PPC64)
#endif
#define SIMDUTF_CAN_ALWAYS_RUN_PPC64 SIMDUTF_IMPLEMENTATION_PPC64 && SIMDUTF_IS_PPC64


#include "simdutf/internal/isadetection.h"

#if SIMDUTF_IMPLEMENTATION_PPC64

namespace simdutf {
/**
 * Implementation for ALTIVEC (PPC64).
 */
namespace ppc64 {
} // namespace ppc64
} // namespace simdutf

#include "simdutf/ppc64/implementation.h"

#include "simdutf/ppc64/begin.h"

// Declarations
#include "simdutf/ppc64/intrinsics.h"
#include "simdutf/ppc64/bitmanipulation.h"
#include "simdutf/ppc64/simd.h"

#include "simdutf/ppc64/end.h"

#endif // SIMDUTF_IMPLEMENTATION_PPC64

#endif // SIMDUTF_PPC64_H
