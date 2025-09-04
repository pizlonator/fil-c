/* x86 CPU feature tuning.
   This file is part of the GNU C Library.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.

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

#define TUNABLE_NAMESPACE cpu
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>		/* Get STDOUT_FILENO for _dl_printf.  */
#include <elf/dl-tunables.h>
#include <string.h>
#include <cpu-features.h>
#include <ldsodefs.h>
#include <dl-tunables-parse.h>
#include <dl-symbol-redir-ifunc.h>

#define CHECK_GLIBC_IFUNC_CPU_OFF(f, cpu_features, name, len)		\
  _Static_assert (sizeof (#name) - 1 == len, #name " != " #len);	\
  if (tunable_str_comma_strcmp_cte (&f, #name))				\
    {									\
      CPU_FEATURE_UNSET (cpu_features, name)				\
      break;								\
    }

#define CHECK_GLIBC_IFUNC_CPU_BOTH(f, cpu_features, name, len)		\
  _Static_assert (sizeof (#name) - 1 == len, #name " != " #len);	\
  if (tunable_str_comma_strcmp_cte (&f, #name))				\
    {									\
      if (f.disable)							\
	CPU_FEATURE_UNSET (cpu_features, name)				\
      else								\
	CPU_FEATURE_SET_ACTIVE (cpu_features, name)			\
      break;								\
    }

/* Disable a preferred feature NAME.  We don't enable a preferred feature
   which isn't available.  */
#define CHECK_GLIBC_IFUNC_PREFERRED_OFF(f, cpu_features, name, len)	\
  _Static_assert (sizeof (#name) - 1 == len, #name " != " #len);	\
  if (tunable_str_comma_strcmp_cte (&f, #name))				\
    {									\
      cpu_features->preferred[index_arch_##name]			\
	&= ~bit_arch_##name;						\
      break;								\
    }

/* Enable/disable a preferred feature NAME.  */
#define CHECK_GLIBC_IFUNC_PREFERRED_BOTH(f, cpu_features, name, len)	\
  _Static_assert (sizeof (#name) - 1 == len, #name " != " #len);	\
  if (tunable_str_comma_strcmp_cte (&f, #name))				\
    {									\
      if (f.disable)							\
	cpu_features->preferred[index_arch_##name] &= ~bit_arch_##name;	\
      else								\
	cpu_features->preferred[index_arch_##name] |= bit_arch_##name;	\
      break;								\
    }

/* Enable/disable a preferred feature NAME.  Enable a preferred feature
   only if the feature NEED is usable.  */
#define CHECK_GLIBC_IFUNC_PREFERRED_NEED_BOTH(f, cpu_features, name,	\
					      need, len)		\
  _Static_assert (sizeof (#name) - 1 == len, #name " != " #len);	\
  if (tunable_str_comma_strcmp_cte (&f, #name))				\
    {									\
      if (f.disable)							\
	cpu_features->preferred[index_arch_##name] &= ~bit_arch_##name;	\
      else if (CPU_FEATURE_USABLE_P (cpu_features, need))		\
	cpu_features->preferred[index_arch_##name] |= bit_arch_##name;	\
      break;								\
    }

attribute_hidden
void
TUNABLE_CALLBACK (set_hwcaps) (tunable_val_t *valp)
{
  /* The current IFUNC selection is based on microbenchmarks in glibc.
     It should give the best performance for most workloads.  But other
     choices may have better performance for a particular workload or on
     the hardware which wasn't available when the selection was made.
     The environment variable:

     GLIBC_TUNABLES=glibc.cpu.hwcaps=-xxx,yyy,-zzz,....

     can be used to enable CPU/ARCH feature yyy, disable CPU/ARCH feature
     yyy and zzz, where the feature name is case-sensitive and has to
     match the ones in cpu-features.h.  It can be used by glibc developers
     to tune for a new processor or override the IFUNC selection to
     improve performance for a particular workload.

     NOTE: the IFUNC selection may change over time.  Please check all
     multiarch implementations when experimenting.  */

  struct cpu_features *cpu_features = &GLRO(dl_x86_cpu_features);

  struct tunable_str_comma_state_t ts;
  tunable_str_comma_init (&ts, valp);

  struct tunable_str_comma_t n;
  while (tunable_str_comma_next (&ts, &n))
    {
      switch (n.len)
	{
	default:
	  break;
	case 3:
	  if (n.disable)
	    {
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX, 3);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, CX8, 3);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, FMA, 3);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, HTT, 3);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, IBT, 3);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, RTM, 3);
	    }
	  break;
	case 4:
	  if (n.disable)
	    {
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX2, 4);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, BMI1, 4);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, BMI2, 4);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, CMOV, 4);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, ERMS, 4);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, FMA4, 4);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, SSE2, 4);
	      CHECK_GLIBC_IFUNC_PREFERRED_OFF (n, cpu_features, I586, 4);
	      CHECK_GLIBC_IFUNC_PREFERRED_OFF (n, cpu_features, I686, 4);
	    }
	  break;
	case 5:
	  {
	    CHECK_GLIBC_IFUNC_CPU_BOTH (n, cpu_features, SHSTK, 5);
	  }
	  if (n.disable)
	    {
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, LZCNT, 5);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, MOVBE, 5);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, SSSE3, 5);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, XSAVE, 5);
	    }
	  break;
	case 6:
	  if (n.disable)
	    {
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, POPCNT, 6);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, SSE4_1, 6);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, SSE4_2, 6);
	      if (memcmp (n.str, "XSAVEC", 6) == 0)
		{
		  /* Update xsave_state_size to XSAVE state size.  */
		  cpu_features->xsave_state_size
		    = cpu_features->xsave_state_full_size;
		  CPU_FEATURE_UNSET (cpu_features, XSAVEC);
		}
	    }
	  break;
	case 7:
	  if (n.disable)
	    {
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX512F, 7);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, OSXSAVE, 7);
	    }
	  break;
	case 8:
	  if (n.disable)
	    {
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX512CD, 8);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX512BW, 8);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX512DQ, 8);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX512ER, 8);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX512PF, 8);
	      CHECK_GLIBC_IFUNC_CPU_OFF (n, cpu_features, AVX512VL, 8);
	    }
	  CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features, Slow_BSF, 8);
	  break;
	case 11:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features, Prefer_ERMS,
						11);
	      CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features, Prefer_FSRM,
						11);
	      CHECK_GLIBC_IFUNC_PREFERRED_NEED_BOTH (n, cpu_features,
						     Slow_SSE4_2,
						     SSE4_2,
						     11);
	    }
	  break;
	case 15:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features,
						Fast_Rep_String, 15);
	    }
	  break;
	case 16:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_NEED_BOTH
		(n, cpu_features, Prefer_No_AVX512, AVX512F, 16);
	    }
	  break;
	case 18:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features,
						Fast_Copy_Backward, 18);
	    }
	  break;
	case 19:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features,
						Fast_Unaligned_Load, 19);
	      CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features,
						Fast_Unaligned_Copy, 19);
	    }
	  break;
	case 20:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_NEED_BOTH
		(n, cpu_features, Prefer_No_VZEROUPPER, AVX, 20);
	    }
	  break;
	case 23:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_NEED_BOTH
		(n, cpu_features, AVX_Fast_Unaligned_Load, AVX, 23);
	    }
	  break;
	case 24:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_NEED_BOTH
		(n, cpu_features, MathVec_Prefer_No_AVX512, AVX512F, 24);
	    }
	  break;
	case 25:
	  {
	    CHECK_GLIBC_IFUNC_PREFERRED_BOTH (n, cpu_features,
					      Avoid_Non_Temporal_Memset, 25);
	  }
	  break;
	case 26:
	    {
	      CHECK_GLIBC_IFUNC_PREFERRED_NEED_BOTH
		(n, cpu_features, Prefer_PMINUB_for_stringop, SSE2, 26);
	    }
	  break;
	}
    }
}

#if CET_ENABLED
attribute_hidden
void
TUNABLE_CALLBACK (set_x86_ibt) (tunable_val_t *valp)
{
  if (tunable_strcmp_cte (valp, "on"))
    GL(dl_x86_feature_control).ibt = cet_always_on;
  else if (tunable_strcmp_cte (valp, "off"))
    GL(dl_x86_feature_control).ibt = cet_always_off;
  else if (tunable_strcmp_cte (valp, "permissive"))
    GL(dl_x86_feature_control).ibt = cet_permissive;
}

attribute_hidden
void
TUNABLE_CALLBACK (set_x86_shstk) (tunable_val_t *valp)
{
  if (tunable_strcmp_cte (valp, "on"))
    GL(dl_x86_feature_control).shstk = cet_always_on;
  else if (tunable_strcmp_cte (valp, "off"))
    GL(dl_x86_feature_control).shstk = cet_always_off;
  else if (tunable_strcmp_cte (valp, "permissive"))
    GL(dl_x86_feature_control).shstk = cet_permissive;
}
#endif
