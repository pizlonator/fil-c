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

/* Set both the input and output baud rates stored in *TERMIOS_P to SPEED.  */
int
__cfsetspeed (struct termios *termios_p, speed_t speed)
{
  int rv;

  rv = __cfsetospeed (termios_p, speed);
  if (rv)
    return rv;

  return __cfsetispeed (termios_p, speed);
}
libc_hidden_def (__cfsetspeed)
weak_alias (__cfsetspeed, cfsetspeed)
