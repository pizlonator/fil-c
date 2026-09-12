/* termios kernel interface for Linux

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

#ifndef K_TERMIOS_H
#define K_TERMIOS_H

#include <stddef.h>
#include <unistd.h>
#include <termios.h>
#include <kernel-features.h>
#include <termios_arch.h>

/* The the termios2 structure used in the kernel interfaces is not the
   same as the termios structure we use in the libc.  Therefore we
   must translate it here.  */

struct termios2
{
  tcflag_t c_iflag;		/* input mode flags */
  tcflag_t c_oflag;		/* output mode flags */
  tcflag_t c_cflag;		/* control mode flags */
  tcflag_t c_lflag;		/* local mode flags */
#if _HAVE_TERMIOS2_C_CC_BEFORE_C_LINE
  cc_t c_cc[_TERMIOS2_NCCS];	/* control characters */
  cc_t c_line;			/* line discipline */
#else
  cc_t c_line;			/* line discipline */
  cc_t c_cc[_TERMIOS2_NCCS];	/* control characters */
#endif
  speed_t c_ispeed;		/* input speed */
  speed_t c_ospeed;		/* output speed */
};

/* Alpha got termios2 late, but TCGETS has exactly the same structure
   format and function as TCGETS2. On all other platforms, the termios2
   interface exists as far back as this version of glibc supports.

   For TCSETS* it is more complicated; this is handled in tcsetattr.c.

   Some other architectures only have the equivalent of the termios2
   interface, in which case the old ioctl names are the only ones
   presented, but are equivalent to the new ones. */
#ifndef TCGETS2
# define TCGETS2  TCGETS
# define TCSETS2  TCSETS
# define TCSETSW2 TCSETSW
# define TCSETSF2 TCSETSF
#elif !__ASSUME_TERMIOS2
/* Hack for Alpha */
# undef  TCGETS2
# define TCGETS2 TCGETS
#endif

#endif /* KTERMIOS_H */
