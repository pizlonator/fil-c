/* Test header that defines feature-marking macros used in some test
   assembly files where sysdep.h cannot be included for some reason.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#ifdef __aarch64__
/* GNU_PROPERTY_AARCH64_* macros from elf.h for use in asm code.  */
#define FEATURE_1_AND 0xc0000000
#define FEATURE_1_BTI 1
#define FEATURE_1_PAC 2
#define FEATURE_1_GCS 4

#define GNU_PROPERTY(type, value)	\
  .section .note.gnu.property, "a";	\
  .p2align 3;				\
  .word 4;				\
  .word 16;				\
  .word 5;				\
  .asciz "GNU";				\
  .word type;				\
  .word 4;				\
  .word value;				\
  .word 0;				\
  .text

#ifdef __ARM_BUILDATTR64_FV
/* Add AArch64 feature bits build attributes.  */
# define FEATURE_1_AND_MARK(value)					\
    .aeabi_subsection aeabi_feature_and_bits, optional, ULEB128;	\
    .if ((value) & FEATURE_1_BTI);					\
    .aeabi_attribute Tag_Feature_BTI, 1;				\
    .else;								\
    .aeabi_attribute Tag_Feature_BTI, 0;				\
    .endif;								\
    .if ((value) & FEATURE_1_GCS);					\
    .aeabi_attribute Tag_Feature_GCS, 1;				\
    .else;								\
    .aeabi_attribute Tag_Feature_GCS, 0;				\
    .endif;								\
    .if ((value) & FEATURE_1_PAC);					\
    .aeabi_attribute Tag_Feature_PAC, 1;				\
    .else;								\
    .aeabi_attribute Tag_Feature_PAC, 0;				\
    .endif;								\
    .text
#else
/* Add a NT_GNU_PROPERTY_TYPE_0 note.  */
# define FEATURE_1_AND_MARK(value) GNU_PROPERTY (FEATURE_1_AND, value)
#endif /* __ARM_BUILDATTR64_FV */

/* Add marking with the supported features to all asm code where this header
   is included.  */
FEATURE_1_AND_MARK (FEATURE_1_BTI | FEATURE_1_PAC | FEATURE_1_GCS)
#endif
