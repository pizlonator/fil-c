/* cfsetspeed(), Linux version.
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

#include <termios_internals.h>

/* Set both the input and output baud rates stored in *TERMIOS_P to SPEED.  */
int
__cfsetspeed (struct termios *termios_p, speed_t speed)
{
  tcflag_t cbaud = ___speed_to_cbaud (speed);

  termios_p->c_ospeed = speed;
  termios_p->c_ispeed = speed;
  termios_p->c_cflag &= ~(CBAUD | CIBAUD);
  termios_p->c_cflag |= cbaud | (cbaud << IBSHIFT);

  return 0;
}
libc_hidden_def (__cfsetspeed)
versioned_symbol (libc, __cfsetspeed, cfsetspeed, GLIBC_2_42);

#if _TERMIOS_OLD_COMPAT

int
attribute_compat_text_section
__old_cfsetspeed (old_termios_t *termios_p, speed_t speed)
{
  speed_t real_speed = ___cbaud_to_speed (speed, -1);
  if (real_speed == (speed_t)-1)
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (EINVAL);

#if !_HAVE_STRUCT_OLD_TERMIOS
  /* Otherwise these fields don't exist in old_termios_t */
  termios_p->c_ospeed = real_speed;
  termios_p->c_ispeed = real_speed;
#endif
  termios_p->c_cflag &= ~(CBAUD | CIBAUD);
  termios_p->c_cflag |= speed | (speed << IBSHIFT);

  return 0;
}
compat_symbol (libc, __old_cfsetspeed, cfsetspeed, GLIBC_2_0);

#endif /* _TERMIOS_OLD_COMPAT */
