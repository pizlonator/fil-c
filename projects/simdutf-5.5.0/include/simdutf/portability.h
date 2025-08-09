#ifndef SIMDUTF_PORTABILITY_H
#define SIMDUTF_PORTABILITY_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cfloat>
#include <cassert>
#ifndef _WIN32
// strcasecmp, strncasecmp
#include <strings.h>
#endif

/**
 * We want to check that it is actually a little endian system at
 * compile-time.
 */

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#define SIMDUTF_IS_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(_WIN32)
#define SIMDUTF_IS_BIG_ENDIAN 0
#else
#if defined(__APPLE__) || defined(__FreeBSD__) // defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__
#include <machine/endian.h>
#elif defined(sun) || defined(__sun) // defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/byteorder.h>
#else  // defined(__APPLE__) || defined(__FreeBSD__)

#ifdef __has_include
#if __has_include(<endian.h>)
#include <endian.h>
#endif //__has_include(<endian.h>)
#endif //__has_include

#endif // defined(__APPLE__) || defined(__FreeBSD__)


#ifndef !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__)
#define SIMDUTF_IS_BIG_ENDIAN 0
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SIMDUTF_IS_BIG_ENDIAN 0
#else // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SIMDUTF_IS_BIG_ENDIAN 1
#endif // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#endif // defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__


/**
 * At this point in time, SIMDUTF_IS_BIG_ENDIAN is defined.
 */

#ifdef _MSC_VER
#define SIMDUTF_VISUAL_STUDIO 1
/**
 * We want to differentiate carefully between
 * clang under visual studio and regular visual
 * studio.
 *
 * Under clang for Windows, we enable:
 *  * target pragmas so that part and only part of the
 *     code gets compiled for advanced instructions.
 *
 */
#ifdef __clang__
// clang under visual studio
#define SIMDUTF_CLANG_VISUAL_STUDIO 1
#else
// just regular visual studio (best guess)
#define SIMDUTF_REGULAR_VISUAL_STUDIO 1
#endif // __clang__
#endif // _MSC_VER

#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
// https://en.wikipedia.org/wiki/C_alternative_tokens
// This header should have no effect, except maybe
// under Visual Studio.
#include <iso646.h>
#endif

#if (defined(__x86_64__) || defined(_M_AMD64)) && !defined(_M_ARM64EC)
#define SIMDUTF_IS_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define SIMDUTF_IS_ARM64 1
#elif defined(__PPC64__) || defined(_M_PPC64)
//#define SIMDUTF_IS_PPC64 1
// The simdutf library does yet support SIMD acceleration under
// POWER processors. Please see https://github.com/lemire/simdutf/issues/51
#elif defined(__s390__)
// s390 IBM system. Big endian.
#elif (defined(__riscv) || defined(__riscv__)) && __riscv_xlen == 64
// RISC-V 64-bit
#define SIMDUTF_IS_RISCV64 1

#if __clang_major__ >= 19
// Does the compiler support target regions for RISC-V
#define SIMDUTF_HAS_RVV_TARGET_REGION 1
#endif

#if __riscv_v_intrinsic >= 11000
#define SIMDUTF_HAS_RVV_INTRINSICS 1
#endif

#define SIMDUTF_HAS_ZVBB_INTRINSICS 0 // there is currently no way to detect this

#if SIMDUTF_HAS_RVV_INTRINSICS && __riscv_vector && __riscv_v_min_vlen >= 128 && __riscv_v_elen >= 64
// RISC-V V extension
#define SIMDUTF_IS_RVV 1
#if SIMDUTF_HAS_ZVBB_INTRINSICS && __riscv_zvbb >= 1000000
// RISC-V Vector Basic Bit-manipulation
#define SIMDUTF_IS_ZVBB 1
#endif
#endif

#elif defined(__loongarch_lp64)
// LoongArch 64-bit
#else
// The simdutf library is designed
// for 64-bit processors and it seems that you are not
// compiling for a known 64-bit platform. Please
// use a 64-bit target such as x64 or 64-bit ARM for best performance.
#define SIMDUTF_IS_32BITS 1

// We do not support 32-bit platforms, but it can be
// handy to identify them.
#if defined(_M_IX86) || defined(__i386__)
#define SIMDUTF_IS_X86_32BITS 1
#elif defined(__arm__) || defined(_M_ARM)
#define SIMDUTF_IS_ARM_32BITS 1
#elif defined(__PPC__) || defined(_M_PPC)
#define SIMDUTF_IS_PPC_32BITS 1
#endif

#endif // defined(__x86_64__) || defined(_M_AMD64)

#ifdef SIMDUTF_IS_32BITS
#ifndef SIMDUTF_NO_PORTABILITY_WARNING
// In the future, we may want to warn users of 32-bit systems that
// the simdutf does not support accelerated kernels for such systems.
#endif // SIMDUTF_NO_PORTABILITY_WARNING
#endif // SIMDUTF_IS_32BITS

// this is almost standard?
#define SIMDUTF_STRINGIFY_IMPLEMENTATION_(a) #a
#define SIMDUTF_STRINGIFY(a) SIMDUTF_STRINGIFY_IMPLEMENTATION_(a)

// Our fast kernels require 64-bit systems.
//
// On 32-bit x86, we lack 64-bit popcnt, lzcnt, blsr instructions.
// Furthermore, the number of SIMD registers is reduced.
//
// On 32-bit ARM, we would have smaller registers.
//
// The simdutf users should still have the fallback kernel. It is
// slower, but it should run everywhere.

//
// Enable valid runtime implementations, and select SIMDUTF_BUILTIN_IMPLEMENTATION
//

// We are going to use runtime dispatch.
#ifdef SIMDUTF_IS_X86_64
#ifdef __clang__
// clang does not have GCC push pop
// warning: clang attribute push can't be used within a namespace in clang up
// til 8.0 so SIMDUTF_TARGET_REGION and SIMDUTF_UNTARGET_REGION must be *outside* of a
// namespace.
#define SIMDUTF_TARGET_REGION(T)                                                       \
  _Pragma(SIMDUTF_STRINGIFY(                                                           \
      clang attribute push(__attribute__((target(T))), apply_to = function)))
#define SIMDUTF_UNTARGET_REGION _Pragma("clang attribute pop")
#elif defined(__GNUC__)
// GCC is easier
#define SIMDUTF_TARGET_REGION(T)                                                       \
  _Pragma("GCC push_options") _Pragma(SIMDUTF_STRINGIFY(GCC target(T)))
#define SIMDUTF_UNTARGET_REGION _Pragma("GCC pop_options")
#endif // clang then gcc

#endif // x86

// Default target region macros don't do anything.
#ifndef SIMDUTF_TARGET_REGION
#define SIMDUTF_TARGET_REGION(T)
#define SIMDUTF_UNTARGET_REGION
#endif

// Is threading enabled?
#if defined(_REENTRANT) || defined(_MT)
#ifndef SIMDUTF_THREADS_ENABLED
#define SIMDUTF_THREADS_ENABLED
#endif
#endif

// workaround for large stack sizes under -O0.
// https://github.com/simdutf/simdutf/issues/691
#ifdef __APPLE__
#ifndef __OPTIMIZE__
// Apple systems have small stack sizes in secondary threads.
// Lack of compiler optimization may generate high stack usage.
// Users may want to disable threads for safety, but only when
// in debug mode which we detect by the fact that the __OPTIMIZE__
// macro is not defined.
#undef SIMDUTF_THREADS_ENABLED
#endif
#endif

#ifdef SIMDUTF_VISUAL_STUDIO
// This is one case where we do not distinguish between
// regular visual studio and clang under visual studio.
// clang under Windows has _stricmp (like visual studio) but not strcasecmp (as clang normally has)
#define simdutf_strcasecmp _stricmp
#define simdutf_strncasecmp _strnicmp
#else
// The strcasecmp, strncasecmp, and strcasestr functions do not work with multibyte strings (e.g. UTF-8).
// So they are only useful for ASCII in our context.
// https://www.gnu.org/software/libunistring/manual/libunistring.html#char-_002a-strings
#define simdutf_strcasecmp strcasecmp
#define simdutf_strncasecmp strncasecmp
#endif

#ifdef NDEBUG

#ifdef SIMDUTF_VISUAL_STUDIO
#define SIMDUTF_UNREACHABLE() __assume(0)
#define SIMDUTF_ASSUME(COND) __assume(COND)
#else
#define SIMDUTF_UNREACHABLE() __builtin_unreachable();
#define SIMDUTF_ASSUME(COND) do { if (!(COND)) __builtin_unreachable(); } while (0)
#endif

#else // NDEBUG

#define SIMDUTF_UNREACHABLE() assert(0);
#define SIMDUTF_ASSUME(COND) assert(COND)

#endif


#if defined(__GNUC__) && !defined(__clang__)
#if __GNUC__ >= 11
#define SIMDUTF_GCC11ORMORE 1
#endif //  __GNUC__ >= 11
#endif // defined(__GNUC__) && !defined(__clang__)


#endif // SIMDUTF_PORTABILITY_H
