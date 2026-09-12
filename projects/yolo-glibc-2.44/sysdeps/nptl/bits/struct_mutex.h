/* Default mutex implementation struct definitions.
   Copyright (C) 2019-2026 Free Software Foundation, Inc.
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

#ifndef _THREAD_MUTEX_INTERNAL_H
#define _THREAD_MUTEX_INTERNAL_H 1

/* Generic struct for both POSIX and C11 mutexes.  New ports are expected
   to use the default layout, however architecture can redefine it to
   add arch-specific extension.  The struct have a size of 32 bytes on LP32
   and 40 bytes on LP64 architectures.  */

struct __pthread_mutex_s
{
  int __lock __LOCK_ALIGNMENT;
  unsigned int __count;
  int __owner;
#if __WORDSIZE == 64
  unsigned int __nusers;
#endif
  /* KIND must stay at this position in the structure to maintain
     binary compatibility with static initializers.

     Concurrency notes:
     The __kind of a mutex is initialized either by the static
     PTHREAD_MUTEX_INITIALIZER or by a call to pthread_mutex_init.

     After a mutex has been initialized, the __kind of a mutex is usually not
     changed.  BUT it can be set to -1 in pthread_mutex_destroy.  */
  int __kind;
#if __WORDSIZE != 64
  unsigned int __nusers;
#endif
#if __WORDSIZE == 64
  int __spins;
  __pthread_list_t __list;
# define __PTHREAD_MUTEX_HAVE_PREV      1
#else
  __extension__ union
  {
    int __spins;
    __pthread_slist_t __list;
  };
# define __PTHREAD_MUTEX_HAVE_PREV      0
#endif
};

#if __PTHREAD_MUTEX_HAVE_PREV == 1
# define __PTHREAD_MUTEX_INITIALIZER(__kind) \
  0, 0, 0, 0, __kind, 0, { 0, 0 }
#else
# define __PTHREAD_MUTEX_INITIALIZER(__kind) \
  0, 0, 0, __kind, 0, { 0 }
#endif

#endif
