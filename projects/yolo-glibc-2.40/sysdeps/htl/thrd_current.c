/* C11 threads current thread implementation.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.
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

#include "thrd_priv.h"
#include <ldsodefs.h>

#pragma weak __pthread_self
#pragma weak __pthread_threads

#ifndef SHARED
#pragma weak _dl_pthread_threads
#endif

thrd_t
thrd_current (void)
{
  if (GL (dl_pthread_threads))
    return (thrd_t) __pthread_self ();

  return (thrd_t) 0;
}
