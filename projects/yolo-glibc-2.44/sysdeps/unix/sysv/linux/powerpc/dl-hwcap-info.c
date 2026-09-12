/* HWCAP string information.  PowerPC version.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.

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

#include <array_length.h>
#include <dl-hwcap-info.h>

const char __dl_hwcap_names[] = {
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

const struct hwcap_info_t __dl_hwcap_info[] =
{
   /* AT_HWCAP tunable masks.  */
   { PPC_FEATURE_HAS_4xxMAC,                 AT_HWCAP },
   { PPC_FEATURE_HAS_ALTIVEC,                AT_HWCAP },
   { PPC_FEATURE_ARCH_2_05,                  AT_HWCAP },
   { PPC_FEATURE_ARCH_2_06,                  AT_HWCAP },
   { PPC_FEATURE_PSERIES_PERFMON_COMPAT,     AT_HWCAP },
   { PPC_FEATURE_BOOKE,                      AT_HWCAP },
   { PPC_FEATURE_CELL_BE,                    AT_HWCAP },
   { PPC_FEATURE_HAS_DFP,                    AT_HWCAP },
   { PPC_FEATURE_HAS_EFP_DOUBLE,             AT_HWCAP },
   { PPC_FEATURE_HAS_EFP_SINGLE,             AT_HWCAP },
   { PPC_FEATURE_HAS_FPU,                    AT_HWCAP },
   { PPC_FEATURE_ICACHE_SNOOP,               AT_HWCAP },
   { PPC_FEATURE_HAS_MMU,                    AT_HWCAP },
   { PPC_FEATURE_NO_TB,                      AT_HWCAP },
   { PPC_FEATURE_PA6T,                       AT_HWCAP },
   { PPC_FEATURE_POWER4,                     AT_HWCAP },
   { PPC_FEATURE_POWER5,                     AT_HWCAP },
   { PPC_FEATURE_POWER5_PLUS,                AT_HWCAP },
   { PPC_FEATURE_POWER6_EXT,                 AT_HWCAP },
   { PPC_FEATURE_32,                         AT_HWCAP },
   { PPC_FEATURE_601_INSTR,                  AT_HWCAP },
   { PPC_FEATURE_64,                         AT_HWCAP },
   { PPC_FEATURE_PPC_LE,                     AT_HWCAP },
   { PPC_FEATURE_SMT,                        AT_HWCAP },
   { PPC_FEATURE_HAS_SPE,                    AT_HWCAP },
   { PPC_FEATURE_TRUE_LE,                    AT_HWCAP },
   { PPC_FEATURE_UNIFIED_CACHE,              AT_HWCAP },
   { PPC_FEATURE_HAS_VSX,                    AT_HWCAP },

   /* AT_HWCAP2 tunable masks.  */
   { PPC_FEATURE2_ARCH_2_07,                 AT_HWCAP2 },
   { PPC_FEATURE2_HAS_DSCR,                  AT_HWCAP2 },
   { PPC_FEATURE2_HAS_EBB,                   AT_HWCAP2 },
   { PPC_FEATURE2_HAS_HTM,                   AT_HWCAP2 },
   { PPC_FEATURE2_HTM_NOSC,                  AT_HWCAP2 },
   { PPC_FEATURE2_HTM_NO_SUSPEND,            AT_HWCAP2 },
   { PPC_FEATURE2_HAS_ISEL,                  AT_HWCAP2 },
   { PPC_FEATURE2_HAS_TAR,                   AT_HWCAP2 },
   { PPC_FEATURE2_HAS_VEC_CRYPTO,            AT_HWCAP2 },
   { PPC_FEATURE2_ARCH_3_00,                 AT_HWCAP2 },
   { PPC_FEATURE2_HAS_IEEE128,               AT_HWCAP2 },
   { PPC_FEATURE2_DARN,                      AT_HWCAP2 },
   { PPC_FEATURE2_SCV,                       AT_HWCAP2 },
   { PPC_FEATURE2_ARCH_3_1,                  AT_HWCAP2 },
   { PPC_FEATURE2_MMA,                       AT_HWCAP2 },
};

size_t __dl_hwcap_info_size = array_length (__dl_hwcap_info);
