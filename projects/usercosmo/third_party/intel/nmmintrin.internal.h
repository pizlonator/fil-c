#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#ifndef _NMMINTRIN_H_INCLUDED
#define _NMMINTRIN_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <nmmintrin.h>
#else /* !__FILC__ */
#include "third_party/intel/smmintrin.internal.h"
#endif
#endif

#endif /* __FILC__ */
