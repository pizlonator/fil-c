/* Allocate a new stack.  Mach version.
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

#include <errno.h>

#include <mach.h>

#include <pt-internal.h>
#include <ldsodefs.h>

/* Allocate a new stack of size STACKSIZE.  If successful, store the
   address of the newly allocated stack in *STACKADDR and return 0.
   Otherwise return an error code (EINVAL for an invalid stack size,
   EAGAIN if the system lacked the necessary resources to allocate a
   new stack).  */
int
__pthread_stack_alloc (void **stackaddr, size_t stacksize)
{
  error_t err;
  vm_prot_t prot = VM_PROT_READ | VM_PROT_WRITE;

  if (GL(dl_stack_prot_flags) & PROT_EXEC)
    prot |= VM_PROT_EXECUTE;

  err = __vm_map (__mach_task_self (), (vm_offset_t *) stackaddr,
		  stacksize, 0, TRUE, MEMORY_OBJECT_NULL, 0, FALSE,
		  prot, VM_PROT_ALL, VM_INHERIT_COPY);

  if (err == KERN_NO_SPACE)
    err = EAGAIN;
  return err;
}
libc_hidden_def (__pthread_stack_alloc)
