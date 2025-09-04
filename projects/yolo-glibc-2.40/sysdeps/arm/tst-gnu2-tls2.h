/* Test TLSDESC relocation.  ARM version.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#include <config.h>
#include <sys/auxv.h>
#include <string.h>
#include <stdlib.h>
#include <endian.h>

#ifndef __SOFTFP__

# ifdef HAVE_ARM_PCS_VFP_D32
#  define SAVE_VFP_D32					\
      asm volatile ("vldr d16,=17" : : : "d16");	\
      asm volatile ("vldr d17,=18" : : : "d17");	\
      asm volatile ("vldr d18,=19" : : : "d18");	\
      asm volatile ("vldr d19,=20" : : : "d19");	\
      asm volatile ("vldr d20,=21" : : : "d20");	\
      asm volatile ("vldr d21,=22" : : : "d21");	\
      asm volatile ("vldr d22,=23" : : : "d22");	\
      asm volatile ("vldr d23,=24" : : : "d23");	\
      asm volatile ("vldr d24,=25" : : : "d24");	\
      asm volatile ("vldr d25,=26" : : : "d25");	\
      asm volatile ("vldr d26,=27" : : : "d26");	\
      asm volatile ("vldr d27,=28" : : : "d27");	\
      asm volatile ("vldr d28,=29" : : : "d28");	\
      asm volatile ("vldr d29,=30" : : : "d29");	\
      asm volatile ("vldr d30,=31" : : : "d30");	\
      asm volatile ("vldr d31,=32" : : : "d31");
# else
#  define SAVE_VFP_D32
# endif

# define INIT_TLSDESC_CALL()				\
  unsigned long hwcap = getauxval (AT_HWCAP)

/* Set each vector register to a value from 1 to 32 before the TLS access,
   dump to memory after TLS access, and compare with the expected values.  */

# define BEFORE_TLSDESC_CALL()				\
  if (hwcap & HWCAP_ARM_VFP)				\
    {							\
      asm volatile ("vldr  d0,=1" : : : "d0");		\
      asm volatile ("vldr  d1,=2" : : : "d1");		\
      asm volatile ("vldr  d2,=3" : : : "d1");		\
      asm volatile ("vldr  d3,=4" : : : "d3");		\
      asm volatile ("vldr  d4,=5" : : : "d4");		\
      asm volatile ("vldr  d5,=6" : : : "d5");		\
      asm volatile ("vldr  d6,=7" : : : "d6");		\
      asm volatile ("vldr  d7,=8" : : : "d7");		\
      asm volatile ("vldr  d8,=9" : : : "d8");		\
      asm volatile ("vldr  d9,=10" : : : "d9");		\
      asm volatile ("vldr d10,=11" : : : "d10");	\
      asm volatile ("vldr d11,=12" : : : "d11");	\
      asm volatile ("vldr d12,=13" : : : "d12");	\
      asm volatile ("vldr d13,=14" : : : "d13");	\
      asm volatile ("vldr d14,=15" : : : "d14");	\
      asm volatile ("vldr d15,=16" : : : "d15");	\
    }							\
  if (hwcap & HWCAP_ARM_VFPD32)				\
    {							\
      SAVE_VFP_D32					\
    }

# define VFP_STACK_REQ (16*8)
# if __BYTE_ORDER == __BIG_ENDIAN
#  define DISP 7
# else
#  define DISP 0
# endif

# ifdef HAVE_ARM_PCS_VFP_D32
#  define CHECK_VFP_D32							\
      char vfp[VFP_STACK_REQ];						\
      asm volatile ("vstmia %0, {d16-d31}\n"				\
		    :							\
		    : "r" (vfp)						\
		    : "memory");					\
									\
      char expected[VFP_STACK_REQ] = { 0 };				\
      for (int i = 0; i < 16; ++i)					\
	expected[i * 8 + DISP] = i + 17;				\
									\
      if (memcmp (vfp, expected, VFP_STACK_REQ) != 0)			\
        abort ();
# else
#  define CHECK_VFP_D32
# endif

# define AFTER_TLSDESC_CALL()						\
  if (hwcap & HWCAP_ARM_VFP)						\
    {									\
      char vfp[VFP_STACK_REQ];						\
      asm volatile ("vstmia %0, {d0-d15}\n"				\
		    :							\
		    : "r" (vfp)						\
		    : "memory");					\
									\
      char expected[VFP_STACK_REQ] = { 0 };				\
      for (int i = 0; i < 16; ++i)					\
	expected[i * 8 + DISP] = i + 1;					\
									\
      if (memcmp (vfp, expected, VFP_STACK_REQ) != 0)			\
        abort ();							\
    }									\
  if (hwcap & HWCAP_ARM_VFPD32)						\
    {									\
      CHECK_VFP_D32							\
    }

#endif /* __SOFTFP__ */

#include_next <tst-gnu2-tls2.h>
