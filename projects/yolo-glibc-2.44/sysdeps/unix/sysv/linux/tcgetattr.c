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

#include <termios_internals.h>

/* Put the state of FD into *TERMIOS_P.  */
int
__tcgetattr (int fd, struct termios *termios_p)
{
  struct termios2 k_termios;
  long int retval = INLINE_SYSCALL_CALL (ioctl, fd, TCGETS2, &k_termios);

  if (__glibc_likely (retval != -1))
    {
      ___termios2_canonicalize_speeds (&k_termios);

      memset (termios_p, 0, sizeof (*termios_p));
      termios_p->c_iflag  = k_termios.c_iflag;
      termios_p->c_oflag  = k_termios.c_oflag;
      termios_p->c_cflag  = k_termios.c_cflag;
      termios_p->c_lflag  = k_termios.c_lflag;
      termios_p->c_line   = k_termios.c_line;
      termios_p->c_ospeed = k_termios.c_ospeed;
      termios_p->c_ispeed = k_termios.c_ispeed;

      copy_c_cc (termios_p->c_cc, NCCS, k_termios.c_cc, _TERMIOS2_NCCS);
    }

  return retval;
}
libc_hidden_def (__tcgetattr)

#if _TERMIOS_OLD_COMPAT && _HAVE_STRUCT_OLD_TERMIOS

versioned_symbol (libc, __tcgetattr, tcgetattr, GLIBC_2_42);

/* Legacy version for shorter struct termios */
int
attribute_compat_text_section
__old_tcgetattr (int fd, old_termios_t *termios_p)
{
  struct termios new_termios;
  int retval = __tcgetattr (fd, &new_termios);
  if (__glibc_likely (retval != -1))
    {
      memcpy (termios_p, &new_termios, sizeof (*termios_p));
    }
  return retval;
}
compat_symbol (libc, __old_tcgetattr, tcgetattr, GLIBC_2_0);

#else

weak_alias (__tcgetattr, tcgetattr)

#endif
