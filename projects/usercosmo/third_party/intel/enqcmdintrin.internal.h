#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#ifndef _X86GPRINTRIN_H_INCLUDED
# error "Never use <enqcmdintrin.h> directly; include <x86gprintrin.h> instead."
#endif
#ifndef _ENQCMDINTRIN_H_INCLUDED
#define _ENQCMDINTRIN_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <enqcmdintrin.h>
#else /* !__FILC__ */
#ifndef __ENQCMD__
#pragma GCC push_options
#pragma GCC target ("enqcmd")
#define __DISABLE_ENQCMD__
#endif
extern __inline int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_enqcmd (void * __P, const void * __Q)
{
  return __builtin_ia32_enqcmd (__P, __Q);
}
extern __inline int
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_enqcmds (void * __P, const void * __Q)
{
  return __builtin_ia32_enqcmds (__P, __Q);
}
#ifdef __DISABLE_ENQCMD__
#undef __DISABLE_ENQCMD__
#pragma GCC pop_options
#endif
#endif
#endif

#endif /* __FILC__ */
