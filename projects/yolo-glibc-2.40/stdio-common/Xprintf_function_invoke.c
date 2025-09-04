/* Invoke a printf specifier handler.  Generic version.
   Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <array_length.h>

int
Xprintf (function_invoke) (void *buf,
                           printf_function callback,
                           union printf_arg *args_value,
                           size_t ndata_args,
                           struct printf_info *info)
{
  /* Most custom specifiers expect just one argument.  Use the heap
     for larger argument arrays.  */
  const void *onstack_args[4];
  const void **args;
  if (ndata_args <= array_length (onstack_args))
    args = onstack_args;
  else
    {
      args = calloc (ndata_args, sizeof (*args));
      if (args == NULL)
        return -1;
    }

  for (unsigned int i = 0; i < ndata_args; ++i)
    args[i] = &args_value[i];

  struct Xprintf (buffer_as_file) s;
  Xprintf (buffer_as_file_init) (&s, buf);

  /* Call the function.  */
  int done = callback (Xprintf (buffer_as_file_get) (&s), info, args);

  if (!Xprintf (buffer_as_file_terminate) (&s))
    done = -1;

  if (args != onstack_args)
    free (args);

  /* Potential error from the callback function.  */
  return done;
}
