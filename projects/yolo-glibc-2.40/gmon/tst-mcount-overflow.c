/* Test program to trigger mcount overflow in profiling collection.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

/* Program with sufficiently complex, yet pointless, call graph
   that it will trigger an mcount overflow, when you set the
   minarcs/maxarcs tunables to very low values. */

#define PREVENT_TAIL_CALL asm volatile ("")

/* Calls REP(n) macro 16 times, for n=0..15.
 * You need to define REP(n) before using this.
 */
#define REPS \
  REP(0) REP(1) REP(2) REP(3) REP(4) REP(5) REP(6) REP(7) \
  REP(8) REP(9) REP(10) REP(11) REP(12) REP(13) REP(14) REP(15)

/* Defines 16 leaf functions named f1_0 to f1_15 */
#define REP(n) \
  __attribute__ ((noinline, noclone, weak)) void f1_##n (void) {};
REPS
#undef REP

/* Calls all 16 leaf functions f1_* in succession */
__attribute__ ((noinline, noclone, weak)) void
f2 (void)
{
# define REP(n) f1_##n();
  REPS
# undef REP
  PREVENT_TAIL_CALL;
}

/* Defines 16 functions named f2_0 to f2_15, which all just call f2 */
#define REP(n) \
  __attribute__ ((noinline, noclone, weak)) void \
  f2_##n (void) { f2(); PREVENT_TAIL_CALL; };
REPS
#undef REP

__attribute__ ((noinline, noclone, weak)) void
f3 (int count)
{
  for (int i = 0; i < count; ++i)
    {
      /* Calls f1_0(), f2_0(), f1_1(), f2_1(), f3_0(), etc */
#     define REP(n) f1_##n(); f2_##n();
      REPS
#     undef REP
    }
}

int
main (void)
{
  f3 (1000);
  return 0;
}
