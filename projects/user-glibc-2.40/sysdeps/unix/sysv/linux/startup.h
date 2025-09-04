/* Linux definitions of functions used by static libc main startup.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

#ifdef SHARED
# include_next <startup.h>
#else
# include <sysdep.h>

/* Avoid a run-time invocation of strlen.  */
#define _startup_fatal(message)                                         \
  do                                                                    \
    {                                                                   \
      size_t __message_length = __builtin_strlen (message);             \
      if (! __builtin_constant_p (__message_length))                    \
        {                                                               \
          extern void _startup_fatal_not_constant (void);               \
          _startup_fatal_not_constant ();                               \
        }                                                               \
      INTERNAL_SYSCALL_CALL (write, STDERR_FILENO, (message),           \
                             __message_length);                         \
      INTERNAL_SYSCALL_CALL (exit_group, 127);                          \
    }                                                                   \
  while (0)
#endif  /* !SHARED */
