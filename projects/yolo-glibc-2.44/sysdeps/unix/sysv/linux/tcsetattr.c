/* Copyright (C) 1993-2026 Free Software Foundation, Inc.
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

#define static_assert_equal(x,y) _Static_assert ((x) == (y), #x " != " #y)

/* Set the state of FD to *TERMIOS_P.  */
int
__tcsetattr (int fd, int optional_actions, const struct termios *termios_p)
{
  struct termios2 k_termios;
  unsigned long cmd;

  memset (&k_termios, 0, sizeof k_termios);

  k_termios.c_iflag = termios_p->c_iflag;
  k_termios.c_oflag = termios_p->c_oflag;
  k_termios.c_cflag = termios_p->c_cflag;
  k_termios.c_lflag = termios_p->c_lflag;
  k_termios.c_line  = termios_p->c_line;

  k_termios.c_ospeed = termios_p->c_ospeed;
  k_termios.c_ispeed = termios_p->c_ispeed;

  ___termios2_canonicalize_speeds (&k_termios);

  copy_c_cc (k_termios.c_cc, _TERMIOS2_NCCS, termios_p->c_cc, NCCS);

  static_assert_equal(TCSADRAIN, TCSANOW + 1);
  static_assert_equal(TCSAFLUSH, TCSANOW + 2);
  static_assert_equal(TCSETSW2,  TCSETS2 + 1);
  static_assert_equal(TCSETSF2,  TCSETS2 + 2);
  static_assert_equal(TCSETSW,   TCSETS  + 1);
  static_assert_equal(TCSETSF,   TCSETS  + 2);

  cmd = (long)optional_actions - TCSANOW;
  if (cmd > 2)
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (EINVAL);

  /* For compatibility with broken workaround hacks for the lack
     of arbitrary speed support in previous versions of glibc,
     clear CIBAUD if only one speed is used.

     This is also necessary for the Alpha compatibility hack below. */
  if (k_termios.c_ospeed == k_termios.c_ispeed)
    k_termios.c_cflag &= ~CIBAUD;

  /* Choose the proper ioctl number to invoke.

     Alpha got TCSETS2 late (Linux 4.20), but has the same structure
     format, and it only needs TCSETS2 if either it needs to use
     __BOTHER or split speed.  All other architectures have TCSETS2 as
     far back as the current glibc supports.  Calling TCSETS with
     __BOTHER causes unpredictable results on old Alpha kernels and
     could even crash them. */
  if (__ASSUME_TERMIOS2 ||
      k_termios.c_ospeed != k_termios.c_ispeed ||
      cbaud (k_termios.c_cflag) == __BOTHER)
    cmd += TCSETS2;
  else
    cmd += TCSETS;

  return INLINE_SYSCALL_CALL (ioctl, fd, cmd, &k_termios);
}
libc_hidden_def (__tcsetattr)

#if _HAVE_STRUCT_OLD_TERMIOS && _TERMIOS_OLD_COMPAT

versioned_symbol (libc, __tcsetattr, tcsetattr, GLIBC_2_42);

/* Legacy version for shorter struct termios without speed fields */
int
attribute_compat_text_section
__old_tcsetattr (int fd, int optional_actions, const old_termios_t *termios_p)
{
  struct termios new_termios;

  memset (&new_termios, 0, sizeof (new_termios));
  new_termios.c_iflag  = termios_p->c_iflag;
  new_termios.c_oflag  = termios_p->c_oflag;
  new_termios.c_cflag  = termios_p->c_cflag;
  new_termios.c_lflag  = termios_p->c_lflag;
  new_termios.c_line   = termios_p->c_line;
  copy_c_cc(new_termios.c_cc, NCCS, termios_p->c_cc, OLD_NCCS);

  return __tcsetattr (fd, optional_actions, &new_termios);
}
compat_symbol (libc, __old_tcsetattr, tcsetattr, GLIBC_2_0);

#else

weak_alias (__tcsetattr, tcsetattr)

#endif
