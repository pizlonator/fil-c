/* Internal sigset_t definition.
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

#ifndef _INTERNAL_SIGSET_H
#define _INTERNAL_SIGSET_H

#include <sigsetops.h>

typedef struct _internal_sigset_t
{
  unsigned long int __val[__NSIG_WORDS];
} internal_sigset_t;

static inline void
internal_sigset_from_sigset (internal_sigset_t *iset, const sigset_t *set)
{
  int cnt = __NSIG_WORDS;
  while (--cnt >= 0)
   iset->__val[cnt] = set->__val[cnt];
}

static inline void
internal_sigemptyset (internal_sigset_t *set)
{
  int cnt = __NSIG_WORDS;
  while (--cnt >= 0)
   set->__val[cnt] = 0;
}

static inline void
internal_sigfillset (internal_sigset_t *set)
{
  int cnt = __NSIG_WORDS;
  while (--cnt >= 0)
   set->__val[cnt] = ~0UL;
}

static inline int
internal_sigisemptyset (const internal_sigset_t *set)
{
  int cnt = __NSIG_WORDS;
  unsigned long int ret = set->__val[--cnt];
  while (ret == 0 && --cnt >= 0)
    ret = set->__val[cnt];
  return ret == 0;
}

static inline void
internal_sigandset (internal_sigset_t *dest, const internal_sigset_t *left,
		    const internal_sigset_t *right)
{
  int cnt = __NSIG_WORDS;
  while (--cnt >= 0)
    dest->__val[cnt] = left->__val[cnt] & right->__val[cnt];
}

static inline void
internal_sigorset (internal_sigset_t *dest, const internal_sigset_t *left,
		   const internal_sigset_t *right)
{
  int cnt = __NSIG_WORDS;
  while (--cnt >= 0)
    dest->__val[cnt] = left->__val[cnt] | right->__val[cnt];
}

static inline int
internal_sigismember (const internal_sigset_t *set, int sig)
{
  unsigned long int mask = __sigmask (sig);
  int word = __sigword (sig);
  return set->__val[word] & mask ? 1 : 0;
}

static inline void
internal_sigaddset (internal_sigset_t *set, int sig)
{
  unsigned long int mask = __sigmask (sig);
  int word = __sigword (sig);
  set->__val[word] |= mask;
}

static inline void
internal_sigdelset (internal_sigset_t *set, int sig)
{
  unsigned long int mask = __sigmask (sig);
  int word = __sigword (sig);
  set->__val[word] &= ~mask;
}

#endif /* _INTERNAL_SIGSET_H */
