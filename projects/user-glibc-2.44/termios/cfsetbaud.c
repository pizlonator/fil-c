/* Copyright (C) 1992-2026 Free Software Foundation, Inc.
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

#include <termios.h>
#include <errno.h>
#include <stddef.h>

/* Set both the input and output baud rates stored in *TERMIOS_P to BAUD.  */
int
__cfsetbaud (struct termios *termios_p, baud_t baud)
{
  return __cfsetspeed (termios_p, baud);
}
libc_hidden_def (__cfsetbaud)
weak_alias (__cfsetbaud, cfsetbaud)
