/* Utility functions for SME tests.
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
   <https://www.gnu.org/licenses/>.  */

struct blk {
  void *za_save_buffer;
  uint16_t num_za_save_slices;
  char __reserved[6];
};

/* Read SVCR to get SM (bit0) and ZA (bit1) state.  */
static unsigned long __attribute_maybe_unused__
get_svcr (void)
{
  register unsigned long x0 asm ("x0");
  asm volatile (
    ".inst   0xd53b4240  /* mrs     x0, svcr  */\n"
    : "=r" (x0));
  return x0;
}

/* Returns tpidr2.  */
static __attribute_maybe_unused__ void *
get_tpidr2 (void)
{
  register unsigned long x0 asm ("x0");
  asm volatile (
    ".inst   0xd53bd0a0  /* mrs     x0, tpidr2_el0  */\n"
    : "=r"(x0) :: "memory");
  return (void *) x0;
}

/* Obtains current streaming SVE vector register size.  */
static unsigned long __attribute_maybe_unused__
get_svl (void)
{
  register unsigned long x0 asm ("x0");
  asm volatile (
    ".inst   0x04bf5820  /* rdsvl   x0, 1  */\n"
    : "=r" (x0));
  return x0;
}

/* PSTATE.ZA = 1, set ZA state to active.  */
static void __attribute_maybe_unused__
start_za (void)
{
  asm volatile (
    ".inst   0xd503457f  /* smstart za  */");
}

/* Load data into ZA byte by byte from p.  */
static void __attribute_maybe_unused__ __attribute__ ((noinline))
load_za (const void *buf, unsigned long svl)
{
  register unsigned long x15 asm ("x15") = 0;
  register unsigned long x16 asm ("x16") = (unsigned long)buf;
  register unsigned long x17 asm ("x17") = svl;

  asm volatile (
    ".inst   0xd503437f  /* smstart sm  */\n"
    ".L_ldr_loop:\n"
    ".inst   0xe1006200  /* ldr     za[w15, 0], [x16]  */\n"
    "add     w15, w15, 1\n"
    ".inst   0x04305030  /* addvl   x16, x16, 1  */\n"
    "cmp     w15, w17\n"
    "bne     .L_ldr_loop\n"
    ".inst   0xd503427f  /* smstop  sm  */\n"
    : "+r"(x15), "+r"(x16), "+r"(x17));
}

/* Set tpidr2 to BLK.  */
static void __attribute_maybe_unused__
set_tpidr2 (struct blk *blk)
{
  register unsigned long x0 asm ("x0") = (unsigned long)blk;
  asm volatile (
    ".inst   0xd51bd0a0  /* msr     tpidr2_el0, x0  */\n"
    :: "r"(x0) : "memory");
}
