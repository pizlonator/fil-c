#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#ifndef _X86GPRINTRIN_H_INCLUDED
# error "Never use <xsavecintrin.h> directly; include <x86gprintrin.h> instead."
#endif
#ifndef _XSAVECINTRIN_H_INCLUDED
#define _XSAVECINTRIN_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <xsavecintrin.h>
#else /* !__FILC__ */
#ifndef __XSAVEC__
#pragma GCC push_options
#pragma GCC target("xsavec")
#define __DISABLE_XSAVEC__
#endif
extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_xsavec (void *__P, long long __M)
{
  __builtin_ia32_xsavec (__P, __M);
}
#ifdef __x86_64__
extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_xsavec64 (void *__P, long long __M)
{
  __builtin_ia32_xsavec64 (__P, __M);
}
#endif
#ifdef __DISABLE_XSAVEC__
#undef __DISABLE_XSAVEC__
#pragma GCC pop_options
#endif
#endif
#endif

#endif /* __FILC__ */
