/* Copyright (C) 1997-2024 Free Software Foundation, Inc.
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

#include <libc-lock.h>
#include <errno.h>
#include <hurd.h>
#include <stdlib.h>
#include <pthreadP.h>

/* Placeholder for key creation routine from Hurd cthreads library.  */
int
weak_function
__cthread_keycreate (__cthread_key_t *key)
{
  *key = -1;
  return __hurd_fail (ENOSYS);
}

/* Placeholder for key retrieval routine from Hurd cthreads library.  */
int
weak_function
__cthread_getspecific (__cthread_key_t key, void **pval)
{
  *pval = NULL;
  return __hurd_fail (ENOSYS);
}

/* Placeholder for key setting routine from Hurd cthreads library.  */
int
weak_function
__cthread_setspecific (__cthread_key_t key, void *val)
{
  return __hurd_fail (ENOSYS);
}
