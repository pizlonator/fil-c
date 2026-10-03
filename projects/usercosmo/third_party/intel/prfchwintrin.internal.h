#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#if !defined _IMMINTRIN_H_INCLUDED && !defined _MM3DNOW_H_INCLUDED
# error "Never use <prfchwintrin.h> directly; include <immintrin.h> or <mm3dnow.h> instead."
#endif
#ifndef _PRFCHWINTRIN_H_INCLUDED
#define _PRFCHWINTRIN_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <prfchwintrin.h>
#else /* !__FILC__ */
extern __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
_m_prefetchw (void *__P)
{
  __builtin_prefetch (__P, 1, 3 );
}
#endif
#endif

#endif /* __FILC__ */
