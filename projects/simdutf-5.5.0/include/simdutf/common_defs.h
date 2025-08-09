#ifndef SIMDUTF_COMMON_DEFS_H
#define SIMDUTF_COMMON_DEFS_H

#include <cassert>
#include "simdutf/portability.h"
#include "simdutf/avx512.h"


#if defined(__GNUC__)
  // Marks a block with a name so that MCA analysis can see it.
  #define SIMDUTF_BEGIN_DEBUG_BLOCK(name) __asm volatile("# LLVM-MCA-BEGIN " #name);
  #define SIMDUTF_END_DEBUG_BLOCK(name) __asm volatile("# LLVM-MCA-END " #name);
  #define SIMDUTF_DEBUG_BLOCK(name, block) BEGIN_DEBUG_BLOCK(name); block; END_DEBUG_BLOCK(name);
#else
  #define SIMDUTF_BEGIN_DEBUG_BLOCK(name)
  #define SIMDUTF_END_DEBUG_BLOCK(name)
  #define SIMDUTF_DEBUG_BLOCK(name, block)
#endif

// Align to N-byte boundary
#define SIMDUTF_ROUNDUP_N(a, n) (((a) + ((n)-1)) & ~((n)-1))
#define SIMDUTF_ROUNDDOWN_N(a, n) ((a) & ~((n)-1))

#define SIMDUTF_ISALIGNED_N(ptr, n) (((uintptr_t)(ptr) & ((n)-1)) == 0)

#if defined(SIMDUTF_REGULAR_VISUAL_STUDIO)
  #define SIMDUTF_DEPRECATED __declspec(deprecated)


  #define simdutf_really_inline __forceinline
  #define simdutf_never_inline __declspec(noinline)

  #define simdutf_unused
  #define simdutf_warn_unused

  #ifndef simdutf_likely
  #define simdutf_likely(x) x
  #endif
  #ifndef simdutf_unlikely
  #define simdutf_unlikely(x) x
  #endif

  #define SIMDUTF_PUSH_DISABLE_WARNINGS __pragma(warning( push ))
  #define SIMDUTF_PUSH_DISABLE_ALL_WARNINGS __pragma(warning( push, 0 ))
  #define SIMDUTF_DISABLE_VS_WARNING(WARNING_NUMBER) __pragma(warning( disable : WARNING_NUMBER ))
  // Get rid of Intellisense-only warnings (Code Analysis)
  // Though __has_include is C++17, it is supported in Visual Studio 2017 or better (_MSC_VER>=1910).
  #ifdef __has_include
  #if __has_include(<CppCoreCheck\Warnings.h>)
  #include <CppCoreCheck\Warnings.h>
  #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS SIMDUTF_DISABLE_VS_WARNING(ALL_CPPCORECHECK_WARNINGS)
  #endif
  #endif

  #ifndef SIMDUTF_DISABLE_UNDESIRED_WARNINGS
  #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS
  #endif

  #define SIMDUTF_DISABLE_DEPRECATED_WARNING SIMDUTF_DISABLE_VS_WARNING(4996)
  #define SIMDUTF_DISABLE_STRICT_OVERFLOW_WARNING
  #define SIMDUTF_POP_DISABLE_WARNINGS __pragma(warning( pop ))

#else // SIMDUTF_REGULAR_VISUAL_STUDIO
#if defined(__OPTIMIZE__) || defined(NDEBUG)
  #define simdutf_really_inline inline __attribute__((always_inline))
#else
  #define simdutf_really_inline inline
#endif

  #define SIMDUTF_DEPRECATED __attribute__((deprecated))
  #define simdutf_never_inline inline __attribute__((noinline))

  #define simdutf_unused __attribute__((unused))
  #define simdutf_warn_unused __attribute__((warn_unused_result))

  #ifndef simdutf_likely
  #define simdutf_likely(x) __builtin_expect(!!(x), 1)
  #endif
  #ifndef simdutf_unlikely
  #define simdutf_unlikely(x) __builtin_expect(!!(x), 0)
  #endif

  #define SIMDUTF_PUSH_DISABLE_WARNINGS _Pragma("GCC diagnostic push")
  // gcc doesn't seem to disable all warnings with all and extra, add warnings here as necessary
  #define SIMDUTF_PUSH_DISABLE_ALL_WARNINGS SIMDUTF_PUSH_DISABLE_WARNINGS \
    SIMDUTF_DISABLE_GCC_WARNING(-Weffc++) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wall) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wconversion) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wextra) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wattributes) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wimplicit-fallthrough) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wnon-virtual-dtor) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wreturn-type) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wshadow) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wunused-parameter) \
    SIMDUTF_DISABLE_GCC_WARNING(-Wunused-variable)
  #define SIMDUTF_PRAGMA(P) _Pragma(#P)
  #define SIMDUTF_DISABLE_GCC_WARNING(WARNING) SIMDUTF_PRAGMA(GCC diagnostic ignored #WARNING)
  #if defined(SIMDUTF_CLANG_VISUAL_STUDIO)
  #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS SIMDUTF_DISABLE_GCC_WARNING(-Wmicrosoft-include)
  #else
  #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS
  #endif
  #define SIMDUTF_DISABLE_DEPRECATED_WARNING SIMDUTF_DISABLE_GCC_WARNING(-Wdeprecated-declarations)
  #define SIMDUTF_DISABLE_STRICT_OVERFLOW_WARNING SIMDUTF_DISABLE_GCC_WARNING(-Wstrict-overflow)
  #define SIMDUTF_POP_DISABLE_WARNINGS _Pragma("GCC diagnostic pop")



#endif // MSC_VER

#ifndef SIMDUTF_DLLIMPORTEXPORT
    #if defined(SIMDUTF_VISUAL_STUDIO)
      /**
       * It does not matter here whether you are using
       * the regular visual studio or clang under visual
       * studio.
       */
      #if SIMDUTF_USING_LIBRARY
      #define SIMDUTF_DLLIMPORTEXPORT __declspec(dllimport)
      #else
      #define SIMDUTF_DLLIMPORTEXPORT __declspec(dllexport)
      #endif
    #else
      #define SIMDUTF_DLLIMPORTEXPORT
    #endif
#endif

/// If EXPR is an error, returns it.
#define SIMDUTF_TRY(EXPR) { auto _err = (EXPR); if (_err) { return _err; } }


#endif // SIMDUTF_COMMON_DEFS_H
