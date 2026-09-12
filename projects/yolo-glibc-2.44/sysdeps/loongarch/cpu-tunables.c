/* LoongArch CPU feature tuning.
   This file is part of the GNU C Library.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.

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
   <http://www.gnu.org/licenses/>.  */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>		/* Get STDOUT_FILENO for _dl_printf.  */
#include <elf/dl-tunables.h>
#include <string.h>
#include <cpu-features.h>
#include <ldsodefs.h>
#include <sys/auxv.h>
#include <dl-tunables-parse.h>
#include <dl-symbol-redir-ifunc.h>

#define CHECK_GLIBC_IFUNC_CPU(f, name, len)			\
  _Static_assert (sizeof (#name) - 1 == len, #name " != " #len);	\
  if (tunable_str_comma_strcmp_cte (&f, #name))				\
    {									\
      if (f.disable)							\
	GLRO(dl_larch_cpu_features).hwcap &= (~HWCAP_LOONGARCH_##name);	\
      else								\
	GLRO(dl_larch_cpu_features).hwcap |= HWCAP_LOONGARCH_##name;	\
      break;								\
    }

attribute_hidden void
TUNABLE_CALLBACK (set_hwcaps) (tunable_val_t *valp)
{
  /* The current IFUNC selection is based on microbenchmarks in glibc.
     It should give the best performance for most workloads.  But other
     choices may have better performance for a particular workload or on
     the hardware which wasn't available when the selection was made.
     The environment variable:

     GLIBC_TUNABLES=glibc.cpu.hwcaps=-xxx,yyy,-zzz,....

     can be used to enable CPU/ARCH feature yyy, disable CPU/ARCH feature
     xxx and zzz, where the feature name is case-sensitive and has to
     match the ones in cpu-features.h.  It can be used by glibc developers
     to tune for a new processor or override the IFUNC selection to
     improve performance for a particular workload.

     NOTE: the IFUNC selection may change over time.  Please check all
     multiarch implementations when experimenting.  */

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
	  {
	    CHECK_GLIBC_IFUNC_CPU (n, LSX, 3);
	    CHECK_GLIBC_IFUNC_CPU (n, UAL, 3);
	  }
	  break;
	case 4:
	  {
	    CHECK_GLIBC_IFUNC_CPU (n, LASX, 4);
	  }
	  break;
	}
    }

  /* Ensure that the user has not enabled any unsupported features.  */
  GLRO(dl_larch_cpu_features).hwcap &= GLRO(dl_hwcap);
}
