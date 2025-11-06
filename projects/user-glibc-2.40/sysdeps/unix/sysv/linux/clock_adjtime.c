/* clock_adjtime -- tune kernel clock.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <sysdep.h>
#include <sys/timex.h>
#include <kernel-features.h>
#include <pizlonated_syscalls.h>

int
__clock_adjtime64 (const clockid_t clock_id, struct __timex64 *tx64)
{
  return zsys_clock_adjtime (clock_id, tx64);
}

#if __TIMESIZE != 64
libc_hidden_def (__clock_adjtime64)

int
__clock_adjtime (const clockid_t clock_id, struct timex *tx)
{
  struct __timex64 tx64;
  int retval;

  tx64 = valid_timex_to_timex64 (*tx);
  retval = __clock_adjtime64 (clock_id, &tx64);
  if (retval >= 0)
    *tx = valid_timex64_to_timex (tx64);

  return retval;
}
#endif
libc_hidden_def (__clock_adjtime);
strong_alias (__clock_adjtime, clock_adjtime)
