/* Spin locks non-inline functions.
   Copyright (C) 2000-2026 Free Software Foundation, Inc.
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

/* <bits/types/__pthread_spinlock_t.h> declares some extern inline functions.  These
   functions are declared additionally here for use when inlining is
   not possible.  */

#define _FORCE_INLINES
#define __PT_SPIN_INLINE	/* empty */

#include <pthread.h>
#include <shlib-compat.h>

libc_hidden_def (__pthread_spin_destroy)
versioned_symbol (libc, __pthread_spin_destroy, pthread_spin_destroy, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libc, __pthread_spin_destroy, pthread_spin_destroy, GLIBC_2_12);
#endif

libc_hidden_def (__pthread_spin_init)
versioned_symbol (libc, __pthread_spin_init, pthread_spin_init, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libc, __pthread_spin_init, pthread_spin_init, GLIBC_2_12);
#endif

libc_hidden_def (__pthread_spin_trylock)
versioned_symbol (libc, __pthread_spin_trylock, pthread_spin_trylock, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libc, __pthread_spin_trylock, pthread_spin_trylock, GLIBC_2_12);
#endif

libc_hidden_def (__pthread_spin_lock)
versioned_symbol (libc, __pthread_spin_lock, pthread_spin_lock, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libc, __pthread_spin_lock, pthread_spin_lock, GLIBC_2_12);
#endif

libc_hidden_def (__pthread_spin_unlock)
versioned_symbol (libc, __pthread_spin_unlock, pthread_spin_unlock, GLIBC_2_43);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_43)
compat_symbol (libc, __pthread_spin_unlock, pthread_spin_unlock, GLIBC_2_12);
#endif
