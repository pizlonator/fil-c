/* Destroy a semaphore.  Generic version.
   Copyright (C) 2005-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <semaphore.h>
#include <errno.h>

#include <hurd.h>
#include <pt-internal.h>
#include <shlib-compat.h>

int
__sem_destroy (sem_t *sem)
{
  struct new_sem *isem = (struct new_sem *) sem;
  if (
#if USE_64B_ATOMICS_ON_SEM_T
      atomic_load_relaxed (&isem->data) >> SEM_NWAITERS_SHIFT
#else
      atomic_load_relaxed (&isem->value) & SEM_NWAITERS_MASK
      || isem->nwaiters
#endif
      )
    /* There are threads waiting on *SEM.  */
    return __hurd_fail (EBUSY);

  return 0;
}

versioned_symbol (libc, __sem_destroy, sem_destroy, GLIBC_2_43);
# if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libpthread, __sem_destroy, sem_destroy, GLIBC_2_12);
#endif
