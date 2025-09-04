/* Linux internal definitions for ioctl.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <termios.h>

/* The user-visible size of struct termios has changed.  Catch ioctl calls
   using the new-style struct termios, and translate them to old-style.  */
static inline bool
__ioctl_arch (int *r, int fd, unsigned long request, void *arg)
{
  switch (request)
    {
    case TCGETS:
      *r = __tcgetattr (fd, (struct termios *) arg);
      break;

    case TCSETS:
      *r = __tcsetattr (fd, TCSANOW, (struct termios *) arg);
      break;

    case TCSETSW:
      *r = __tcsetattr (fd, TCSADRAIN, (struct termios *) arg);
      break;

    case TCSETSF:
      *r = __tcsetattr (fd, TCSAFLUSH, (struct termios *) arg);
      break;

    default:
      return false;
    }
  return true;
}
