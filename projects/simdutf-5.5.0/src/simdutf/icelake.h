#ifndef SIMDUTF_ICELAKE_H
#define SIMDUTF_ICELAKE_H


#include "simdutf/portability.h"

#ifdef __has_include
// How do we detect that a compiler supports vbmi2?
// For sure if the following header is found, we are ok?
#if __has_include(<avx512vbmi2intrin.h>)
#define SIMDUTF_COMPILER_SUPPORTS_VBMI2 1
#endif
#endif

#ifdef _MSC_VER
#if _MSC_VER >= 1930
// Visual Studio 2022 and up support VBMI2 under x64 even if the header
// avx512vbmi2intrin.h is not found.
// Visual Studio 2019 technically supports VBMI2, but the implementation
// might be unreliable. Search for visualstudio2019icelakeissue in our
// tests.
#define SIMDUTF_COMPILER_SUPPORTS_VBMI2 1
#endif
#endif

// We allow icelake on x64 as long as the compiler is known to support VBMI2.
#ifndef SIMDUTF_IMPLEMENTATION_ICELAKE
#define SIMDUTF_IMPLEMENTATION_ICELAKE ((SIMDUTF_IS_X86_64) && (SIMDUTF_COMPILER_SUPPORTS_VBMI2))
#endif

// To see why  (__BMI__) && (__LZCNT__) are not part of this next line, see
// https://github.com/simdutf/simdutf/issues/1247
#if ((SIMDUTF_IMPLEMENTATION_ICELAKE) && (SIMDUTF_IS_X86_64) && (__AVX2__) && (SIMDUTF_HAS_AVX512F && \
                                         SIMDUTF_HAS_AVX512DQ && \
                                         SIMDUTF_HAS_AVX512VL && \
                                           SIMDUTF_HAS_AVX512VBMI2) && (!SIMDUTF_IS_32BITS))
#define SIMDUTF_CAN_ALWAYS_RUN_ICELAKE 1
#else
#define SIMDUTF_CAN_ALWAYS_RUN_ICELAKE 0
#endif

#if SIMDUTF_IMPLEMENTATION_ICELAKE
#if SIMDUTF_CAN_ALWAYS_RUN_ICELAKE
#define SIMDUTF_TARGET_ICELAKE
#else
#define SIMDUTF_TARGET_ICELAKE SIMDUTF_TARGET_REGION("avx512f,avx512dq,avx512cd,avx512bw,avx512vbmi,avx512vbmi2,avx512vl,avx2,bmi,bmi2,pclmul,lzcnt,popcnt,avx512vpopcntdq")
#endif

namespace simdutf {
namespace icelake {
} // namespace icelake
} // namespace simdutf



//
// These two need to be included outside SIMDUTF_TARGET_REGION
//
#include "simdutf/icelake/intrinsics.h"
#include "simdutf/icelake/implementation.h"

//
// The rest need to be inside the region
//
#include "simdutf/icelake/begin.h"
// Declarations
#include "simdutf/icelake/bitmanipulation.h"
#include "simdutf/icelake/end.h"



#endif // SIMDUTF_IMPLEMENTATION_ICELAKE
#endif // SIMDUTF_ICELAKE_H
