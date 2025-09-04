/* Initialize cpu feature data.  PowerPC version.
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

#ifndef __CPU_FEATURES_POWERPC_H
# define __CPU_FEATURES_POWERPC_H

#include <stdbool.h>
#include <sys/auxv.h>

struct cpu_features
{
  bool use_cached_memopt;
  unsigned long int hwcap;
  unsigned long int hwcap2;
  unsigned long int hwcap3;
  unsigned long int hwcap4;
};

static const char hwcap_names[] = {
  "4xxmac\0"
  "altivec\0"
  "arch_2_05\0"
  "arch_2_06\0"
  "archpmu\0"
  "booke\0"
  "cellbe\0"
  "dfp\0"
  "efpdouble\0"
  "efpsingle\0"
  "fpu\0"
  "ic_snoop\0"
  "mmu\0"
  "notb\0"
  "pa6t\0"
  "power4\0"
  "power5\0"
  "power5+\0"
  "power6x\0"
  "ppc32\0"
  "ppc601\0"
  "ppc64\0"
  "ppcle\0"
  "smt\0"
  "spe\0"
  "true_le\0"
  "ucache\0"
  "vsx\0"
  "arch_2_07\0"
  "dscr\0"
  "ebb\0"
  "htm\0"
  "htm-nosc\0"
  "htm-no-suspend\0"
  "isel\0"
  "tar\0"
  "vcrypto\0"
  "arch_3_00\0"
  "ieee128\0"
  "darn\0"
  "scv\0"
  "arch_3_1\0"
  "mma\0"
};

static const struct
{
  unsigned int mask;
  bool id;
} hwcap_tunables[] = {
   /* AT_HWCAP tunable masks.  */
   { PPC_FEATURE_HAS_4xxMAC,                 0 },
   { PPC_FEATURE_HAS_ALTIVEC,                0 },
   { PPC_FEATURE_ARCH_2_05,                  0 },
   { PPC_FEATURE_ARCH_2_06,                  0 },
   { PPC_FEATURE_PSERIES_PERFMON_COMPAT,     0 },
   { PPC_FEATURE_BOOKE,                      0 },
   { PPC_FEATURE_CELL_BE,                    0 },
   { PPC_FEATURE_HAS_DFP,                    0 },
   { PPC_FEATURE_HAS_EFP_DOUBLE,             0 },
   { PPC_FEATURE_HAS_EFP_SINGLE,             0 },
   { PPC_FEATURE_HAS_FPU,                    0 },
   { PPC_FEATURE_ICACHE_SNOOP,               0 },
   { PPC_FEATURE_HAS_MMU,                    0 },
   { PPC_FEATURE_NO_TB,                      0 },
   { PPC_FEATURE_PA6T,                       0 },
   { PPC_FEATURE_POWER4,                     0 },
   { PPC_FEATURE_POWER5,                     0 },
   { PPC_FEATURE_POWER5_PLUS,                0 },
   { PPC_FEATURE_POWER6_EXT,                 0 },
   { PPC_FEATURE_32,                         0 },
   { PPC_FEATURE_601_INSTR,                  0 },
   { PPC_FEATURE_64,                         0 },
   { PPC_FEATURE_PPC_LE,                     0 },
   { PPC_FEATURE_SMT,                        0 },
   { PPC_FEATURE_HAS_SPE,                    0 },
   { PPC_FEATURE_TRUE_LE,                    0 },
   { PPC_FEATURE_UNIFIED_CACHE,              0 },
   { PPC_FEATURE_HAS_VSX,                    0 },

   /* AT_HWCAP2 tunable masks.  */
   { PPC_FEATURE2_ARCH_2_07,                 1 },
   { PPC_FEATURE2_HAS_DSCR,                  1 },
   { PPC_FEATURE2_HAS_EBB,                   1 },
   { PPC_FEATURE2_HAS_HTM,                   1 },
   { PPC_FEATURE2_HTM_NOSC,                  1 },
   { PPC_FEATURE2_HTM_NO_SUSPEND,            1 },
   { PPC_FEATURE2_HAS_ISEL,                  1 },
   { PPC_FEATURE2_HAS_TAR,                   1 },
   { PPC_FEATURE2_HAS_VEC_CRYPTO,            1 },
   { PPC_FEATURE2_ARCH_3_00,                 1 },
   { PPC_FEATURE2_HAS_IEEE128,               1 },
   { PPC_FEATURE2_DARN,                      1 },
   { PPC_FEATURE2_SCV,                       1 },
   { PPC_FEATURE2_ARCH_3_1,                  1 },
   { PPC_FEATURE2_MMA,                       1 },
};

#endif /* __CPU_FEATURES_H  */
