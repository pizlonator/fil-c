/* Scrub and reseed the AT_RANDOM bytes.  Linux version.
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

#ifndef _DL_RESEED_RANDOM_H
#define _DL_RESEED_RANDOM_H

#include <not-cancel.h>
#include <sys/random.h>

/* The stack and pointer guards have been derived from the 16 AT_RANDOM
   bytes pointed to by DL_RANDOM.  Overwrite them in place with fresh entropy
   unrelated to the guards, so the value returned by getauxval (AT_RANDOM) no
   longer reveals them while still being random.

   This is best-effort and must not perturb process startup.  The getrandom
   might not provide all the requested entropy, and leaving the original bytes
   is deliberate: AT_RANDOM is exposed through getauxval, and a potential
   leak of the guards is preferable to scrubbing the value to a predictable
   constant.  */
static inline void __attribute__ ((always_inline))
_dl_reseed_random (void **dl_random)
{
  if (*dl_random == NULL)
    return;
  __getrandom_nocancel_nostatus_direct (*dl_random, 16, GRND_NONBLOCK);
  *dl_random = NULL;
}

#endif /* _DL_RESEED_RANDOM_H */
