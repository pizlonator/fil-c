#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#ifndef _X86GPRINTRIN_H_INCLUDED
# error "Never use <serializeintrin.h> directly; include <x86gprintrin.h> instead."
#endif
#ifndef _SERIALIZE_H_INCLUDED
#define _SERIALIZE_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <serializeintrin.h>
#else /* !__FILC__ */
#ifndef __SERIALIZE__
#pragma GCC push_options
#pragma GCC target("serialize")
#define __DISABLE_SERIALIZE__
#endif
extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_serialize (void)
{
  __builtin_ia32_serialize ();
}
#ifdef __DISABLE_SERIALIZE__
#undef __DISABLE_SERIALIZE__
#pragma GCC pop_options
#endif
#endif
#endif

#endif /* __FILC__ */
