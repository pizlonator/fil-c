#ifndef SIMDUTF_HASWELL_INTRINSICS_H
#define SIMDUTF_HASWELL_INTRINSICS_H

#include "simdutf.h"

#ifdef SIMDUTF_VISUAL_STUDIO
// under clang within visual studio, this will include <x86intrin.h>
#include <intrin.h>  // visual studio or clang
#else

#if SIMDUTF_GCC11ORMORE
// We should not get warnings while including <x86intrin.h> yet we do
// under some versions of GCC.
// If the x86intrin.h header has uninitialized values that are problematic,
// it is a GCC issue, we want to ignore these warnings.
SIMDUTF_DISABLE_GCC_WARNING(-Wuninitialized)
#endif

#include <x86intrin.h> // elsewhere


#if SIMDUTF_GCC11ORMORE
// cancels the suppression of the -Wuninitialized
SIMDUTF_POP_DISABLE_WARNINGS
#endif

#endif // SIMDUTF_VISUAL_STUDIO

#ifdef SIMDUTF_CLANG_VISUAL_STUDIO
/**
 * You are not supposed, normally, to include these
 * headers directly. Instead you should either include intrin.h
 * or x86intrin.h. However, when compiling with clang
 * under Windows (i.e., when _MSC_VER is set), these headers
 * only get included *if* the corresponding features are detected
 * from macros:
 * e.g., if __AVX2__ is set... in turn,  we normally set these
 * macros by compiling against the corresponding architecture
 * (e.g., arch:AVX2, -mavx2, etc.) which compiles the whole
 * software with these advanced instructions. In simdutf, we
 * want to compile the whole program for a generic target,
 * and only target our specific kernels. As a workaround,
 * we directly include the needed headers. These headers would
 * normally guard against such usage, but we carefully included
 * <x86intrin.h>  (or <intrin.h>) before, so the headers
 * are fooled.
 */
#include <bmiintrin.h>   // for _blsr_u64
#include <lzcntintrin.h> // for  __lzcnt64
#include <immintrin.h>   // for most things (AVX2, AVX512, _popcnt64)
#include <smmintrin.h>
#include <tmmintrin.h>
#include <avxintrin.h>
#include <avx2intrin.h>
// unfortunately, we may not get _blsr_u64, but, thankfully, clang
// has it as a macro.
#ifndef _blsr_u64
// we roll our own
#define _blsr_u64(n) ((n - 1) & n)
#endif //  _blsr_u64
#endif // SIMDUTF_CLANG_VISUAL_STUDIO

#endif // SIMDUTF_HASWELL_INTRINSICS_H
