/* Initialize cpu feature data.  s390x version.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <elf/dl-tunables.h>
#include <ifunc-memcmp.h>
#include <string.h>
#include <dl-symbol-redir-ifunc.h>
#include <dl-tunables-parse.h>

#define S390_COPY_CPU_FEATURES(SRC_PTR, DEST_PTR)	\
  (DEST_PTR)->hwcap = (SRC_PTR)->hwcap;			\
  (DEST_PTR)->stfle_bits[0] = (SRC_PTR)->stfle_bits[0];

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
	  stfle_bits0_mask = S390_STFLE_MASK_ARCH13_MIE3;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "z13")
	       || tunable_str_comma_strcmp_cte (&t, "arch11"))
	{
	  reset_features = true;
	  disable = true;
	  hwcap_mask = HWCAP_S390_VXRS_EXT | HWCAP_S390_VXRS_EXT2;
	  stfle_bits0_mask = S390_STFLE_MASK_ARCH13_MIE3;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "z14")
	       || tunable_str_comma_strcmp_cte (&t, "arch12"))
	{
	  reset_features = true;
	  disable = true;
	  hwcap_mask = HWCAP_S390_VXRS_EXT2;
	  stfle_bits0_mask = S390_STFLE_MASK_ARCH13_MIE3;
	}
      else if (tunable_str_comma_strcmp_cte (&t, "z15")
	       || tunable_str_comma_strcmp_cte (&t, "z16")
	       || tunable_str_comma_strcmp_cte (&t, "arch13")
	       || tunable_str_comma_strcmp_cte (&t, "arch14"))
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
	stfle_bits0_mask = S390_STFLE_MASK_ARCH13_MIE3;

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
	    cpu_features_curr.stfle_bits[0] &= ~stfle_bits0_mask;
	  else
	    cpu_features_curr.stfle_bits[0] |= stfle_bits0_mask;
	}
    }

  /* Copy back the features after checking that no unsupported features were
     enabled by user.  */
  cpu_features->hwcap = cpu_features_curr.hwcap & cpu_features_orig.hwcap;
  cpu_features->stfle_bits[0] = cpu_features_curr.stfle_bits[0]
    & cpu_features_orig.stfle_bits[0];
}

static inline void
init_cpu_features (struct cpu_features *cpu_features)
{
  /* Fill cpu_features as passed by kernel and machine.  */
  cpu_features->hwcap = GLRO(dl_hwcap);

  /* We want just 1 double word to be returned.  */
  if (__glibc_likely ((cpu_features->hwcap & HWCAP_S390_STFLE)
		      && (cpu_features->hwcap & HWCAP_S390_ZARCH)
		      && (cpu_features->hwcap & HWCAP_S390_HIGH_GPRS)))
    {
      register unsigned long reg0 __asm__("0") = 0;
      __asm__ __volatile__(".machine push"        "\n\t"
			   ".machine \"z9-109\""  "\n\t"
			   ".machinemode \"zarch_nohighgprs\"\n\t"
			   "stfle %0"             "\n\t"
			   ".machine pop"         "\n"
			   : "=QS" (cpu_features->stfle_bits[0]),
			     "+d" (reg0)
			   : : "cc");
    }
  else
    {
      cpu_features->stfle_bits[0] = 0ULL;
    }

  TUNABLE_GET (glibc, cpu, hwcaps, tunable_val_t *, TUNABLE_CALLBACK (set_hwcaps));
}
