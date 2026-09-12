/* Set current context.  OpenRISC version.
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

#include <shlib-compat.h>
#include <sysdep.h>
#include "ucontext_i.h"

#define __CONTEXT_FUNC_NAME __setcontext
#define __CONTEXT_ENABLE_FPCSR 1
#define __CONTEXT_SIGMASK_OFFSET UCONTEXT_SIGMASK
#define __STARTCONTEXT_FUNC_NAME __startcontext

#include "setcontext-common.S"

versioned_symbol (libc, __setcontext, setcontext, GLIBC_2_40)

#if SHLIB_COMPAT (libc, GLIBC_2_35, GLIBC_2_40)

/* Define a compat version of setcontext for glibc's before the fpcsr
   field was added to mcontext_t.  The offset sigmask changed with this
   introduction, the change was done because glibc's definition of
   ucontext_t was initially defined incompatible with the Linux
   definition of ucontext_t.  We keep the compatability definition to
   allow getcontext, setcontext and swapcontext to work in older
   binaries.  */

# undef __CONTEXT_FUNC_NAME
# undef __CONTEXT_ENABLE_FPCSR
# undef __CONTEXT_SIGMASK_OFFSET
# undef __STARTCONTEXT_FUNC_NAME
# define __CONTEXT_FUNC_NAME __setcontext_nofpcsr
# define __CONTEXT_SIGMASK_OFFSET (UCONTEXT_SIGMASK - 4)
# define __STARTCONTEXT_FUNC_NAME __startcontext_nofpcsr

# include "setcontext-common.S"

compat_symbol (libc, __setcontext_nofpcsr, setcontext, GLIBC_2_35)

#endif
