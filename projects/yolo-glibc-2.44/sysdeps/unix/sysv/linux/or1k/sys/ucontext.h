/* ucontext_t definition, OpenRISC version.
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

/* System V/OpenRISC compliant context switching support.  */

#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H	1

#include <features.h>

#include <bits/types/sigset_t.h>
#include <bits/types/stack_t.h>

/* Number of general registers.  */
#define __NGREG	32
#ifdef __USE_MISC
# define NGREG	__NGREG
#endif

/* Context to describe whole processor state.  */
typedef struct
  {
    unsigned long int __gprs[__NGREG];
    unsigned long int __pc;
    unsigned long int __sr;
    unsigned long int __fpcsr;
  } mcontext_t;

/* Userlevel context.  */
typedef struct ucontext_t
  {
    unsigned long int __uc_flags;
    struct ucontext_t *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
  } ucontext_t;

#endif /* sys/ucontext.h */
