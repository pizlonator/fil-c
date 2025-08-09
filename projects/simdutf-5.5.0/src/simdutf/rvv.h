#ifndef SIMDUTF_RVV_H
#define SIMDUTF_RVV_H

#ifdef SIMDUTF_FALLBACK_H
#error "rvv.h must be included before fallback.h"
#endif

#include "simdutf/portability.h"

#define SIMDUTF_CAN_ALWAYS_RUN_RVV SIMDUTF_IS_RVV

#ifndef SIMDUTF_IMPLEMENTATION_RVV
#define SIMDUTF_IMPLEMENTATION_RVV (SIMDUTF_CAN_ALWAYS_RUN_RVV || (SIMDUTF_IS_RISCV64 && SIMDUTF_HAS_RVV_INTRINSICS && SIMDUTF_HAS_RVV_TARGET_REGION))
#endif

#if SIMDUTF_IMPLEMENTATION_RVV

#if SIMDUTF_CAN_ALWAYS_RUN_RVV
#define SIMDUTF_TARGET_RVV
#else
#define SIMDUTF_TARGET_RVV SIMDUTF_TARGET_REGION("arch=+v")
#endif
#if !SIMDUTF_IS_ZVBB && SIMDUTF_HAS_ZVBB_INTRINSICS
#define SIMDUTF_TARGET_ZVBB SIMDUTF_TARGET_REGION("arch=+v,+zvbb")
#endif

namespace simdutf {
namespace rvv {
} // namespace rvv
} // namespace simdutf

#include "simdutf/rvv/implementation.h"
#include "simdutf/rvv/begin.h"
#include "simdutf/rvv/intrinsics.h"
#include "simdutf/rvv/end.h"

#endif // SIMDUTF_IMPLEMENTATION_RVV

#endif // SIMDUTF_RVV_H
