/* Initialize cpu feature data.  s390x version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

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

#include <cpu-features.h>
#include <ldsodefs.h>
#include <sys/auxv.h>
#include <elf/dl-tunables.h>
#include <ifunc-memcmp.h>
#include <string.h>
#include <dl-symbol-redir-ifunc.h>
#include <dl-tunables-parse.h>

#define S390_COPY_CPU_FEATURES(SRC_PTR, DEST_PTR)	\
  (DEST_PTR)->hwcap = (SRC_PTR)->hwcap;			\
  (DEST_PTR)->stfle_filtered = (SRC_PTR)->stfle_filtered;

static void
TUNABLE_CALLBACK (set_hwcaps) (tunable_val_t *valp)
{
  /* The current IFUNC selection is always using the most recent
     features which are available via AT_HWCAP or STFLE-bits.  But in
     some scenarios it is useful to adjust this selection.

     The environment variable:

     GLIBC_TUNABLES=glibc.cpu.hwcaps=-xxx,yyy,zzz,....

     can be used to enable HWCAP/STFLE feature yyy, disable HWCAP/STFLE feature
     xxx, where the feature name is case-sensitive and has to match the ones
     used below.  Furthermore, the ARCH-level zzz can be used to set various
     HWCAP/STFLE features at once.  */

  /* Copy the features from dl_s390_cpu_features, which contains the features
     provided by AT_HWCAP and stfle-instruction.  */
  struct cpu_features *cpu_features = &GLRO(dl_s390_cpu_features);
  struct cpu_features cpu_features_orig;
  S390_COPY_CPU_FEATURES (cpu_features, &cpu_features_orig);
  struct cpu_features cpu_features_curr;
  S390_COPY_CPU_FEATURES (cpu_features, &cpu_features_curr);

  struct tunable_str_comma_state_t ts;
  tunable_str_comma_init (&ts, valp);

  struct tunable_str_comma_t t;
  while (tunable_str_comma_next (&ts, &t))
    {
      if (t.len == 0)
	continue;

      /* Handle only the features here which are really used in the
	 IFUNC-resolvers.  All others are ignored as the values are only used
	 inside glibc.  */
      bool reset_features = false;
      unsigned long int hwcap_mask = 0UL;
      unsigned long long stfle_bits0_mask = 0ULL;
      bool disable = t.disable;

      if (tunable_str_comma_strcmp_cte (&t, "zEC12")
	  || tunable_str_comma_strcmp_cte (&t, "arch10"))
	{
	  reset_features = true;
	  disable = true;
	  hwcap_mask = HWCAP_S390_VXRS | HWCAP_S390_VXRS_EXT
	    | HWCAP_S390_VXRS_EXT2;
	  stfle_bits0_mask = S390_STFLE_BIT61_ARCH13_MIE3;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "z13")
	       || tunable_str_comma_strcmp_cte (&t, "arch11"))
	{
	  reset_features = true;
	  disable = true;
	  hwcap_mask = HWCAP_S390_VXRS_EXT | HWCAP_S390_VXRS_EXT2;
	  stfle_bits0_mask = S390_STFLE_BIT61_ARCH13_MIE3;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "z14")
	       || tunable_str_comma_strcmp_cte (&t, "arch12"))
	{
	  reset_features = true;
	  disable = true;
	  hwcap_mask = HWCAP_S390_VXRS_EXT2;
	  stfle_bits0_mask = S390_STFLE_BIT61_ARCH13_MIE3;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "z15")
	       || tunable_str_comma_strcmp_cte (&t, "z16")
	       || tunable_str_comma_strcmp_cte (&t, "z17")
	       || tunable_str_comma_strcmp_cte (&t, "arch13")
	       || tunable_str_comma_strcmp_cte (&t, "arch14")
	       || tunable_str_comma_strcmp_cte (&t, "arch15"))
	{
	  /* For z15 or newer we don't have to disable something, but we have
	     to reset to the original values.  */
	  reset_features = true;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "HWCAP_S390_VXRS"))
	{
	  hwcap_mask = HWCAP_S390_VXRS;
	  if (t.disable)
	    hwcap_mask |= HWCAP_S390_VXRS_EXT | HWCAP_S390_VXRS_EXT2;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "HWCAP_S390_VXRS_EXT"))
	{
	  hwcap_mask = HWCAP_S390_VXRS_EXT;
	  if (t.disable)
	    hwcap_mask |= HWCAP_S390_VXRS_EXT2;
	  else
	    hwcap_mask |= HWCAP_S390_VXRS;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "HWCAP_S390_VXRS_EXT2"))
	{
	  hwcap_mask = HWCAP_S390_VXRS_EXT2;
	  if (!t.disable)
	    hwcap_mask |= HWCAP_S390_VXRS | HWCAP_S390_VXRS_EXT;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "STFLE_MIE3"))
	stfle_bits0_mask = S390_STFLE_BIT61_ARCH13_MIE3;

      /* Perform the actions determined above.  */
      if (reset_features)
	{
	  S390_COPY_CPU_FEATURES (&cpu_features_orig, &cpu_features_curr);
	}

      if (hwcap_mask != 0UL)
	{
	  if (disable)
	    cpu_features_curr.hwcap &= ~hwcap_mask;
	  else
	    cpu_features_curr.hwcap |= hwcap_mask;
	}

      if (stfle_bits0_mask != 0ULL)
	{
	  if (disable)
	    cpu_features_curr.stfle_filtered &= ~stfle_bits0_mask;
	  else
	    cpu_features_curr.stfle_filtered |= stfle_bits0_mask;
	}
    }

  /* Copy back the features after checking that no unsupported features were
     enabled by user.  */
  cpu_features->hwcap = cpu_features_curr.hwcap & cpu_features_orig.hwcap;
  cpu_features->stfle_filtered = cpu_features_curr.stfle_filtered
    & cpu_features_orig.stfle_filtered;
}

static inline void
init_cpu_features_no_tunables (struct cpu_features *cpu_features)
{
  /* Only initialize once.  */
  if (cpu_features->hwcap != 0)
    return;

  /* Fill cpu_features as passed by kernel and machine.  */
  cpu_features->hwcap = GLRO(dl_hwcap);

  /* We want just 1 double word to be returned.  */
  if (__glibc_likely ((cpu_features->hwcap & HWCAP_S390_STFLE)))
    {
      unsigned long long stfle_bits[4] = { 0 };
      register unsigned long reg0 __asm__("0") = 3;
      __asm__ __volatile__(".machine push"        "\n\t"
			   ".machine \"z9-109\""  "\n\t"
			   "stfle %0"             "\n\t"
			   ".machine pop"         "\n"
			   : "=QS" (stfle_bits[0]),
			     "+d" (reg0)
			   : : "cc");

      unsigned long long internal_stfle_bits = 0;

      /* Facility bit 34: z10: General instructions extension.  */
      if ((stfle_bits[0] & (1ULL << (63 - 34))) != 0)
	internal_stfle_bits |= S390_STFLE_BIT34_Z10;

      /* Facility bit 45: z196: Distinct operands, popcount, ...  */
      if ((stfle_bits[0] & (1ULL << (63 - 45))) != 0)
	internal_stfle_bits |= S390_STFLE_BIT45_Z196;

      /* Facility bit 61: arch13/z15: Miscellaneous-Instruction-Extensions
	 Facility 3, e.g. mvcrl.  */
      if ((stfle_bits[0] & (1ULL << (63 - 61))) != 0)
	internal_stfle_bits |= S390_STFLE_BIT61_ARCH13_MIE3;

      /* Facility bit 84: arch15/z17: Miscellaneous-instruction-extensions 4  */
      if ((stfle_bits[1] & (1ULL << (127 - 84))) != 0)
	internal_stfle_bits |= S390_STFLE_BIT84_ARCH15_MIE4;

      /* Facility bit 198: arch15/z17: Vector-enhancements-facility 3  */
      if ((stfle_bits[3] & (1ULL << (255 - 198))) != 0)
	internal_stfle_bits |= S390_STFLE_BIT198_ARCH15_VXRS_EXT3;

      /* Facility bit 199: arch15/z17: Vector-Packed-Decimal-Enhancement 3  */
      if ((stfle_bits[3] & (1ULL << (255 - 199))) != 0)
	internal_stfle_bits |= S390_STFLE_BIT199_ARCH15_VXRS_PDE3;

      /* Facility bit 201: arch15/z17: CPU: Concurrent-Functions Facility  */
      if ((stfle_bits[3] & (1ULL << (255 - 201))) != 0)
	internal_stfle_bits |= S390_STFLE_BIT201_ARCH15_CON;

      cpu_features->stfle_orig = internal_stfle_bits;
      cpu_features->stfle_filtered = internal_stfle_bits;
    }
}

static inline void
init_cpu_features (struct cpu_features *cpu_features)
{
  init_cpu_features_no_tunables (cpu_features);

  TUNABLE_GET (glibc, cpu, hwcaps, tunable_val_t *, TUNABLE_CALLBACK (set_hwcaps));
}
