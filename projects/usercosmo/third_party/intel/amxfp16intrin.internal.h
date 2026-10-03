#if defined(__x86_64__) && !(__ASSEMBLER__ + __LINKER__ + 0)
#if !defined _IMMINTRIN_H_INCLUDED
#error "Never use <amxfp16intrin.h> directly; include <immintrin.h> instead."
#endif
#ifndef _AMXFP16INTRIN_H_INCLUDED
#define _AMXFP16INTRIN_H_INCLUDED
#ifdef __FILC__
/* Fil-C port: this is a copy of GCC's intrin header, and its inline
   helpers use GCC-only builtins that clang does not implement.  Under
   Fil-C use clang's own intrinsic headers instead. */
#include <amxfp16intrin.h>
#else /* !__FILC__ */
#if defined(__x86_64__)
#define _tile_dpfp16ps_internal(dst,src1,src2) __asm__ volatile ("{tdpfp16ps\t%%tmm"#src2", %%tmm"#src1", %%tmm"#dst"|tdpfp16ps\t%%tmm"#dst", %%tmm"#src1", %%tmm"#src2"}" ::)
#define _tile_dpfp16ps(dst,src1,src2) _tile_dpfp16ps_internal (dst,src1,src2)
#endif
#ifdef __DISABLE_AMX_FP16__
#undef __DISABLE_AMX_FP16__
#pragma GCC pop_options
#endif
#endif
#endif

#endif /* __FILC__ */
