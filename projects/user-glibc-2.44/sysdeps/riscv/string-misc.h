/* Miscellaneous functions used in string implementations.  RISC-V version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#ifndef _RISCV_STRING_MISC_H
#define _RISCV_STRING_MISC_H 1

#include <limits.h>
#include <endian.h>
#include <string-optype.h>

#if defined __riscv_zbkb
/* Extract the byte at index IDX from word X, with index 0 being the
   least significant byte.  */
static __always_inline unsigned char
extractbyte (op_t x, unsigned int idx)
{
  if (__BYTE_ORDER == __LITTLE_ENDIAN)
    return x >> (idx * CHAR_BIT);
  else
    return x >> (sizeof (x) - 1 - idx) * CHAR_BIT;
}

/* Setup an word with each byte being c_in.  For instance, on a 64 bits
   machine with input as 0xce the functions returns 0xcececececececece.  */
static __always_inline op_t
repeat_bytes (unsigned char c_in)
{
  op_t c = c_in;
#if __riscv_xlen == 64
  /* 8bit -> 16bit -> 32bit -> 64bit pattern replication */
  __asm__ ("packh %0, %1, %1\n\t"
           "packw %0, %0, %0\n\t"
           "pack %0, %0, %0"
           : "=r"(c) : "r"(c));
#elif __riscv_xlen == 32
  /* 8bit -> 16bit -> 32bit pattern replication */
  __asm__ ("packh %0, %1, %1\n\t"
           "pack %0, %0, %0"
           : "=r"(c) : "r"(c));
#else
  #error unsupported xlen
#endif
  return c;
}
#else
#include <sysdeps/generic/string-misc.h>
#endif

#endif /* _RISCV_STRING_MISC_H */
