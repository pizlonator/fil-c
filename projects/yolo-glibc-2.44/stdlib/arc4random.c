/* Pseudo Random Number Generator
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <not-cancel.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/random.h>

static void
arc4random_getrandom_failure (void)
{
  __libc_fatal ("Fatal glibc error: cannot get entropy for arc4random\n");
}

void
__arc4random_buf (void *p, size_t n)
{
  static int seen_initialized;
  ssize_t l;
  int fd;

  if (n == 0)
    return;

  for (;;)
    {
      l = TEMP_FAILURE_RETRY (__getrandom_nocancel (p, n, 0));
      if (l > 0)
	{
	  if ((size_t) l == n)
	    return; /* Done reading, success.  */
	  p = (uint8_t *) p + l;
	  n -= l;
	  continue; /* Interrupted by a signal; keep going.  */
	}
      else if (l < 0 && errno == ENOSYS)
	break; /* No syscall, so fallback to /dev/urandom.  */
      arc4random_getrandom_failure ();
    }

  if (atomic_load_relaxed (&seen_initialized) == 0)
    {
      /* Poll /dev/random as an approximation of RNG initialization.  */
      struct pollfd pfd = { .events = POLLIN };
      pfd.fd = TEMP_FAILURE_RETRY (
	  __open64_nocancel ("/dev/random", O_RDONLY | O_CLOEXEC | O_NOCTTY));
      if (pfd.fd < 0)
	arc4random_getrandom_failure ();
      if (TEMP_FAILURE_RETRY (__poll_infinity_nocancel (&pfd, 1)) < 0)
	arc4random_getrandom_failure ();
      if (__close_nocancel (pfd.fd) < 0)
	arc4random_getrandom_failure ();
      atomic_store_relaxed (&seen_initialized, 1);
    }

  fd = TEMP_FAILURE_RETRY (
      __open64_nocancel ("/dev/urandom", O_RDONLY | O_CLOEXEC | O_NOCTTY));
  if (fd < 0)
    arc4random_getrandom_failure ();
  for (;;)
    {
      l = TEMP_FAILURE_RETRY (__read_nocancel (fd, p, n));
      if (l <= 0)
	arc4random_getrandom_failure ();
      if ((size_t) l == n)
	break; /* Done reading, success.  */
      p = (uint8_t *) p + l;
      n -= l;
    }
  if (__close_nocancel (fd) < 0)
    arc4random_getrandom_failure ();
}
libc_hidden_def (__arc4random_buf)
weak_alias (__arc4random_buf, arc4random_buf)

uint32_t
__arc4random (void)
{
  uint32_t r;
  __arc4random_buf (&r, sizeof (r));
  return r;
}
libc_hidden_def (__arc4random)
weak_alias (__arc4random, arc4random)
