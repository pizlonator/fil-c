/* OpenRISC helper for the clone syscall.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <stdarg.h>
#include <sysdep.h>

extern int __or1k_clone (int (*fn)(void *), void *child_stack,
			 int flags, void *arg, pid_t *ptid,
			 void *tls, pid_t *ctid);


/* The OpenRISC ABI uses the stack for varargs like those using in clone
   but the linux syscall ABI uses registers.
   This function moves from varargs to regs.  */
int
__clone (int (*fn)(void *), void *child_stack,
	 int flags, void *arg, ...
	 /* pid_t *ptid, struct user_desc *tls, pid_t *ctid */ )
{
  void *ptid;
  void *tls;
  void *ctid;
  va_list ap;
  int err;

  va_start (ap, arg);
  ptid = va_arg (ap, void *);
  tls = va_arg (ap, void *);
  ctid = va_arg (ap, void *);
  va_end (ap);

  /* Sanity check the arguments */
  err = -EINVAL;
  if (!fn)
    goto syscall_error;
  if (!child_stack)
    goto syscall_error;

  return __or1k_clone (fn, child_stack, flags, arg, ptid, tls, ctid);

syscall_error:
  __set_errno (-err);
  return -1;
}
libc_hidden_def (__clone)
weak_alias (__clone, clone)
