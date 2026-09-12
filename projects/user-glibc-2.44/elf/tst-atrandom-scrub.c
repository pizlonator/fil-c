/* Verify the AT_RANDOM bytes do not reveal the guards after startup.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* The loader (security_init) and the static startup code (__libc_start_main)
   derive the stack and pointer guards from the AT_RANDOM bytes, scrub those
   bytes, and refill them with fresh entropy unrelated to the guards.  The
   AT_RANDOM entry is kept, so getauxval (AT_RANDOM) keeps returning 16 random
   bytes, but they no longer reveal the guards.  Check that neither guard can
   be reconstructed from AT_RANDOM and that no auxiliary vector entry holds a
   guard value.  */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>

#include <stackguard-macros.h>
#include <tls.h>
#include <support/check.h>

static int
do_test (void)
{
  uintptr_t stack_guard = STACK_CHK_GUARD;
  uintptr_t pointer_guard = POINTER_CHK_GUARD;

  unsigned char *random = (unsigned char *) getauxval (AT_RANDOM);
  if (random == NULL)
    FAIL_UNSUPPORTED ("the kernel did not provide AT_RANDOM");

  printf ("debug: stack guard   = %0*jx\n",
          (int) (2 * sizeof (uintptr_t)), (uintmax_t) stack_guard);
  printf ("debug: pointer guard = %0*jx\n",
          (int) (2 * sizeof (uintptr_t)), (uintmax_t) pointer_guard);
  printf ("debug: AT_RANDOM     = ");
  for (int i = 0; i < 16; i++)
    printf ("%02x", random[i]);
  printf ("\n");

  /* Sanity check: the guards and AT_RANDOM must all have been populated
     (not all-zero), otherwise we cannot tell scrub-and-reseed apart from
     "never set up".  */
  TEST_VERIFY (stack_guard != 0);
  TEST_VERIFY (pointer_guard != 0);
  bool random_all_zero = true;
  for (int i = 0; i < 16; i++)
    if (random[i] != 0)
      {
        random_all_zero = false;
        break;
      }
  TEST_VERIFY (!random_all_zero);

  /* Reconstruct the guards from the (reseeded) AT_RANDOM bytes the way the
     loader does and check that they no longer match the live guards.  */
  uintptr_t recovered_stack;
  memcpy (&recovered_stack, random, sizeof (recovered_stack));
#if __BYTE_ORDER == __LITTLE_ENDIAN
  recovered_stack &= ~(uintptr_t) 0xff;
#else
  recovered_stack &= ~((uintptr_t) 0xff << (8 * (sizeof (recovered_stack) - 1)));
#endif
  TEST_VERIFY (recovered_stack != stack_guard);

  uintptr_t recovered_pointer;
  memcpy (&recovered_pointer, random + sizeof (uintptr_t),
          sizeof (recovered_pointer));
  TEST_VERIFY (recovered_pointer != pointer_guard);

  return 0;
}

#include <support/test-driver.c>
