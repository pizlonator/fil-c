/* `struct termios' speed frobnication functions, baud rate wrappers.
   Any platform which doesn't have Bxxx == xxx for all baud rate
   constants will need to override this file.

   Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

#include <stddef.h>
#include <errno.h>
#include <termios.h>

baud_t
__cfgetobaud (const struct termios *termios_p)
{
  return __cfgetospeed (termios_p);
}
libc_hidden_def (__cfgetobaud)
weak_alias (__cfgetobaud, cfgetobaud)

baud_t
__cfgetibaud (const struct termios *termios_p)
{
  return __cfgetispeed (termios_p);
}
libc_hidden_def (__cfgetibaud)
weak_alias (__cfgetibaud, cfgetibaud)

int
__cfsetobaud (struct termios *termios_p, baud_t baud)
{
  return __cfsetospeed (termios_p, baud);
}
libc_hidden_def (__cfsetobaud)
weak_alias (__cfsetobaud, cfsetobaud)

int
__cfsetibaud (struct termios *termios_p, baud_t baud)
{
  return __cfsetispeed (termios_p, baud);
}
libc_hidden_def (__cfsetibaud)
weak_alias (__cfsetibaud, cfsetibaud)
