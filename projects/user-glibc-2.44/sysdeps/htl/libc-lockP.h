/* Private libc-internal interface for mutex locks.
   Copyright (C) 2015-2026 Free Software Foundation, Inc.
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

#ifndef _BITS_LIBC_LOCKP_H
#define _BITS_LIBC_LOCKP_H 1

#include <pthread.h>

typedef pthread_rwlock_t __libc_rwlock_t;

#define __libc_rwlock_define(CLASS,NAME) \
  CLASS __libc_rwlock_t NAME;
#define __libc_rwlock_define_initialized(CLASS,NAME) \
  CLASS __libc_rwlock_t NAME = PTHREAD_RWLOCK_INITIALIZER;
#define __libc_rwlock_init(NAME) __pthread_rwlock_init (&(NAME), NULL)
#define __libc_rwlock_fini(NAME) ((void) 0)
#define __libc_rwlock_rdlock(NAME) __pthread_rwlock_rdlock (&(NAME))
#define __libc_rwlock_wrlock(NAME) __pthread_rwlock_wrlock (&(NAME))
#define __libc_rwlock_tryrdlock(NAME) __pthread_rwlock_tryrdlock (&(NAME))
#define __libc_rwlock_trywrlock(NAME) __pthread_rwlock_trywrlock (&(NAME))
#define __libc_rwlock_unlock(NAME) __pthread_rwlock_unlock (&(NAME))

/* If we check for a weakly referenced symbol and then perform a
   normal jump to it te code generated for some platforms in case of
   PIC is unnecessarily slow.  What would happen is that the function
   is first referenced as data and then it is called indirectly
   through the PLT.  We can make this a direct jump.  */
#ifdef __PIC__
# define __libc_maybe_call(FUNC, ARGS, ELSE) \
  (__extension__ ({ __typeof (FUNC) *_fn = (FUNC); \
		    _fn != NULL ? (*_fn) ARGS : ELSE; }))
#else
# define __libc_maybe_call(FUNC, ARGS, ELSE) \
  (FUNC != NULL ? FUNC ARGS : ELSE)
#endif

/* Functions that are used by this file and are internal to the GNU C
   library.  */

extern int __pthread_mutex_init (pthread_mutex_t *__mutex,
				 const pthread_mutexattr_t *__mutex_attr);
extern int __pthread_mutex_destroy (pthread_mutex_t *__mutex);
libc_hidden_proto (__pthread_mutex_destroy)

extern int __pthread_mutex_trylock (pthread_mutex_t *__mutex);

extern int __pthread_mutex_lock (pthread_mutex_t *__mutex);

extern int __pthread_mutex_unlock (pthread_mutex_t *__mutex);

extern int __pthread_mutexattr_init (pthread_mutexattr_t *__attr);
libc_hidden_proto (__pthread_mutexattr_init)

extern int __pthread_mutexattr_destroy (pthread_mutexattr_t *__attr);
libc_hidden_proto (__pthread_mutexattr_destroy)

extern int __pthread_rwlock_init (pthread_rwlock_t *__rwlock,
				  const pthread_rwlockattr_t *__attr);
libc_hidden_proto (__pthread_rwlock_init)

extern int __pthread_rwlock_destroy (pthread_rwlock_t *__rwlock);
libc_hidden_proto (__pthread_rwlock_destroy)

extern int __pthread_rwlock_rdlock (pthread_rwlock_t *__rwlock);
libc_hidden_proto (__pthread_rwlock_rdlock)

extern int __pthread_rwlock_tryrdlock (pthread_rwlock_t *__rwlock);
libc_hidden_proto (__pthread_rwlock_tryrdlock)

extern int __pthread_rwlock_wrlock (pthread_rwlock_t *__rwlock);
libc_hidden_proto (__pthread_rwlock_wrlock)

extern int __pthread_rwlock_trywrlock (pthread_rwlock_t *__rwlock);
libc_hidden_proto (__pthread_rwlock_trywrlock)

extern int __pthread_rwlock_unlock (pthread_rwlock_t *__rwlock);
libc_hidden_proto (__pthread_rwlock_unlock)

extern int __pthread_once (pthread_once_t *__once_control,
			   void (*__init_routine) (void));
libc_hidden_proto (__pthread_once);

extern int __pthread_atfork (void (*__prepare) (void),
			     void (*__parent) (void),
			     void (*__child) (void));

extern int __pthread_setcancelstate (int state, int *oldstate);
libc_hidden_proto (__pthread_setcancelstate)
/* Make the pthread functions weak so that we can elide them from
   single-threaded processes.  */
#if !defined(__NO_WEAK_PTHREAD_ALIASES) && !IS_IN (libpthread)
# ifdef weak_extern
weak_extern (__pthread_initialize)
weak_extern (__pthread_atfork)
# else
#  pragma weak __pthread_initialize
#  pragma weak __pthread_atfork
# endif
#endif

#endif	/* bits/libc-lockP.h */
