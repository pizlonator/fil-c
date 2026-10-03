#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#ifndef _X86GPRINTRIN_H_INCLUDED
# error "Never use <wbnoinvdintrin.h> directly; include <x86gprintrin.h> instead."
#endif
#ifndef _WBNOINVDINTRIN_H_INCLUDED
#define _WBNOINVDINTRIN_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <wbnoinvdintrin.h>
#else /* !__FILC__ */
#ifndef __WBNOINVD__
#pragma GCC push_options
#pragma GCC target("wbnoinvd")
#define __DISABLE_WBNOINVD__
#endif
extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_wbnoinvd (void)
{
  __builtin_ia32_wbnoinvd ();
}
#ifdef __DISABLE_WBNOINVD__
#undef __DISABLE_WBNOINVD__
#pragma GCC pop_options
#endif
#endif
#endif

#endif /* __FILC__ */
