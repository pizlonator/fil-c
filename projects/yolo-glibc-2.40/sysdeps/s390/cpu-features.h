/* Initialize cpu feature data.  s390x version.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.

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

#define S390_STFLE_BITS_Z10  34 /* General instructions extension */
#define S390_STFLE_BITS_Z196 45 /* Distinct operands, pop ... */
#define S390_STFLE_BITS_ARCH13_MIE3 61 /* Miscellaneous-Instruction-Extensions
					  Facility 3, e.g. mvcrl.  */

#define S390_STFLE_MASK_ARCH13_MIE3 (1ULL << (63 - S390_STFLE_BITS_ARCH13_MIE3))


#define S390_IS_ARCH13_MIE3(STFLE_BITS_ARRAY)			\
  (((STFLE_BITS_ARRAY)[0] & S390_STFLE_MASK_ARCH13_MIE3) != 0)

#define S390_IS_Z196(STFLE_BITS_ARRAY)			\
  (((STFLE_BITS_ARRAY)[0] & (1ULL << (63 - S390_STFLE_BITS_Z196))) != 0)

#define S390_IS_Z10(STFLE_BITS_ARRAY)				\
  (((STFLE_BITS_ARRAY)[0] & (1ULL << (63 - S390_STFLE_BITS_Z10))) != 0)

struct cpu_features
{
  unsigned long int hwcap;
  unsigned long int __reserved_hwcap2;
  unsigned long long stfle_bits[3];
  unsigned long long __reserved[11];
};

#endif /* __CPU_FEATURES_S390X_H  */
