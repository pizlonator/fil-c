/* Initialize CPU feature data via IFUNC relocation.
   Copyright (C) 2015-2026 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */


#include <ldsodefs.h>

#ifdef SHARED
# include <cpu-features.c>
# include <gcc-macros.h>

/* NB: Normally, DL_PLATFORM_INIT calls init_cpu_features to initialize
   CPU features in dynamic executable.  But when loading ld.so inside of
   static executable, DL_PLATFORM_INIT isn't called and IFUNC relocation
   is used to call init_cpu_features.  In static executable, it is called
   once by IFUNC relocation.  In dynamic executable, it is called twice
   by DL_PLATFORM_INIT and by IFUNC relocation.  */
extern void __x86_cpu_features (void) attribute_hidden;
void (*const __x86_cpu_features_p) (void) attribute_hidden
  = __x86_cpu_features;

void
_dl_x86_init_cpu_features (void)
{
  struct cpu_features *cpu_features = __get_cpu_features ();
  if (cpu_features->basic.kind == arch_kind_unknown)
    {
      init_cpu_features (cpu_features);

# if IS_IN (rtld)
      /* See isa-level.c.  */
#  if defined GCCMACRO__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16		\
  && defined HAVE_X86_LAHF_SAHF && defined GCCMACRO__POPCNT__		\
  && defined GCCMACRO__SSE3__ && defined GCCMACRO__SSSE3__		\
  && defined GCCMACRO__SSE4_1__ && defined GCCMACRO__SSE4_2__
      if (!(cpu_features->isa_1 & GNU_PROPERTY_X86_ISA_1_V2))
	_dl_fatal_printf ("\
Fatal glibc error: CPU does not support x86-64-v%d\n", 2);
#   if defined GCCMACRO__AVX__ && defined GCCMACRO__AVX2__ \
  && defined GCCMACRO__F16C__ && defined GCCMACRO__FMA__   \
  && defined GCCMACRO__LZCNT__ && defined HAVE_X86_MOVBE
      if (!(cpu_features->isa_1 & GNU_PROPERTY_X86_ISA_1_V3))
	_dl_fatal_printf ("\
Fatal glibc error: CPU does not support x86-64-v%d\n", 3);
#    if defined GCCMACRO__AVX512F__ && defined GCCMACRO__AVX512BW__ \
     && defined GCCMACRO__AVX512CD__ && defined GCCMACRO__AVX512DQ__ \
     && defined GCCMACRO__AVX512VL__
      if (!(cpu_features->isa_1 & GNU_PROPERTY_X86_ISA_1_V4))
	_dl_fatal_printf ("\
Fatal glibc error: CPU does not support x86-64-v%d\n", 4);
#    endif /* ISA level 4 */
#   endif /* ISA level 3 */
#  endif /* ISA level 2 */
# ifdef GCCMACRO__APX_F__
      if (!CPU_FEATURE_USABLE_P (cpu_features, APX_F))
	_dl_fatal_printf ("\
Fatal glibc error: CPU does not support APX\n");
# endif
# endif /* IS_IN (rtld) */
    }
}

__ifunc (__x86_cpu_features, __x86_cpu_features, NULL, void,
	 _dl_x86_init_cpu_features);
#endif

#undef _dl_x86_get_cpu_features

const struct cpu_features *
_dl_x86_get_cpu_features (void)
{
  return &GLRO(dl_x86_cpu_features);
}
