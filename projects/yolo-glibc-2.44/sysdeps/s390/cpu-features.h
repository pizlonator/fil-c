/* Initialize cpu feature data.  s390x version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.

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

#ifndef __CPU_FEATURES_S390X_H
# define __CPU_FEATURES_S390X_H

/* The following stfle bit definitions are intended to be used for the
   glibc internal stfle_orig and stfle_filtered fields in cpu_features
   struct.  They can't be used on the double words retrieved by the
   stfle-instruction.  */

/* Facility bit 34: z10: General instructions extension.  */
#define S390_STFLE_BIT34_Z10 (1ULL << 0)

/* Facility bit 45: z196: Distinct operands, popcount, ...  */
#define S390_STFLE_BIT45_Z196 (1ULL << 1)

/* Facility bit 61: arch13/z15: Miscellaneous-Instruction-Extensions
   Facility 3, e.g. mvcrl.  */
#define S390_STFLE_BIT61_ARCH13_MIE3 (1ULL << 2)

/* Facility bit 84: arch15/z17: Miscellaneous-instruction-extensions
   facility 4  */
#define S390_STFLE_BIT84_ARCH15_MIE4 (1ULL << 3)

/* Facility bit 198: arch15/z17: Vector-enhancements-facility 3  */
#define S390_STFLE_BIT198_ARCH15_VXRS_EXT3 (1ULL << 4)

/* Facility bit 199: arch15/z17: Vector-Packed-Decimal-Enhancement
   Facility 3  */
#define S390_STFLE_BIT199_ARCH15_VXRS_PDE3 (1ULL << 5)

/* Facility bit 201: arch15/z17: CPU: Concurrent-Functions Facility  */
#define S390_STFLE_BIT201_ARCH15_CON (1ULL << 6)

#define S390_IS_ARCH15(STFLE_BITS)					\
  ((((STFLE_BITS) & S390_STFLE_BIT84_ARCH15_MIE4) != 0)			\
   && (((STFLE_BITS) & S390_STFLE_BIT198_ARCH15_VXRS_EXT3) != 0)	\
   && (((STFLE_BITS) & S390_STFLE_BIT199_ARCH15_VXRS_PDE3) != 0)	\
   && (((STFLE_BITS) & S390_STFLE_BIT201_ARCH15_CON) != 0))

#define S390_IS_ARCH13_MIE3(STFLE_BITS)			\
  (((STFLE_BITS) & S390_STFLE_BIT61_ARCH13_MIE3) != 0)

#define S390_IS_Z196(STFLE_BITS)		\
  (((STFLE_BITS) & S390_STFLE_BIT45_Z196) != 0)

#define S390_IS_Z10(STFLE_BITS)			\
  (((STFLE_BITS) & S390_STFLE_BIT34_Z10) != 0)

struct cpu_features
{
  unsigned long int hwcap;
  unsigned long int __reserved_hwcap2;
  unsigned long long __reserved;
  unsigned long long stfle_orig;
  unsigned long long stfle_filtered;
  unsigned long long __reserved2[11];
};

#endif /* __CPU_FEATURES_S390X_H  */
