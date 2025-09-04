/* Helper function to allocate shadow stack.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <libc-pointer-arith.h>
#include <allocate-shadow-stack.h>

/* NB: This can be treated as a syscall by caller.  */

long int
__allocate_shadow_stack (size_t stack_size,
			 shadow_stack_size_t *child_stack)
{
#ifdef __NR_map_shadow_stack
  size_t shadow_stack_size
    = stack_size >> STACK_SIZE_TO_SHADOW_STACK_SIZE_SHIFT;
  /* Align shadow stack to 8 bytes.  */
  shadow_stack_size = ALIGN_UP (shadow_stack_size, 8);
  /* Since sigaltstack shares shadow stack with the current context in
     the thread, add extra 20 stack frames in shadow stack for signal
     handlers.  */
  shadow_stack_size += 20 * 8;
  void *shadow_stack = (void *)INLINE_SYSCALL_CALL
    (map_shadow_stack, NULL, shadow_stack_size, SHADOW_STACK_SET_TOKEN);
  /* Report the map_shadow_stack error.  */
  if (shadow_stack == MAP_FAILED)
    return -errno;

  /* Save the shadow stack base and size on child stack.  */
  child_stack[0] = (uintptr_t) shadow_stack;
  child_stack[1] = shadow_stack_size;

  return 0;
#else
  return -ENOSYS;
#endif
}
