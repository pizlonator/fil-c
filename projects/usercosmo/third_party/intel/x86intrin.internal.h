#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#ifndef _X86INTRIN_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <x86intrin.h>
#else /* !__FILC__ */
#include "third_party/intel/x86gprintrin.internal.h"
#ifndef __iamcu__
#include "third_party/intel/immintrin.internal.h"
#include "third_party/intel/mm3dnow.internal.h"
#include "third_party/intel/fma4intrin.internal.h"
#include "third_party/intel/xopintrin.internal.h"
#endif
#endif
#endif

#endif /* __FILC__ */
