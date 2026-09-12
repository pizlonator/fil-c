/* `struct termios' speed frobnication functions.  Linux version.
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
#include <libc-diag.h>

/* Conversions between legacy c_cflag fields and actual baud rates */

/* These expressions may seem complicated; the _cbix() macro
   compresses the CBAUD field into an index in the range 0-31. On most
   Linux platforms, the CBAUD field is 5 bits, but the topmost bit
   indicated by CBAUDEX, is discontinous with the rest.

   The resulting masks look like:

		Alpha		PowerPC		others

   CBAUD	0x001f		0x00ff		0x100f
   CBAUDEX	0x0000		0x0010		0x1000

   LOWCBAUD	0x001f		0x000f		0x000f
   CBAUDMASK	0x001f		0x001f		0x100f

   CBAUDMASK is used to test for invalid values passed to the
   compatibility functions or in termios::c_cflag on PowerPC.

   The divide-multiply sequence in the _cbix() macro gets converted
   to shift and masks as necessary by the compiler. */

#define LOWCBAUD (CBAUD & (CBAUDEX-1))
#define _cbix(x) (((x) & LOWCBAUD) | \
		  (CBAUDEX ? ((x) & CBAUDEX)/CBAUDEX * (LOWCBAUD+1) : 0))
#define CBAUDMASK (LOWCBAUD | CBAUDEX)

/* Compile time sanity checks for broken CBAUD or CIBAUD definitions */
#if CIBAUD != (CBAUD << IBSHIFT)
# error "CIBAUD should == CBAUD << IBSHIFT"
#elif CBAUDEX & (CBAUDEX-1)
# error "CBAUDEX should either be 0 or a single bit"
#elif !(CBAUD & 1)
# error "The CBAUD field should start at bit 0"
#elif CBAUDEX & ~CBAUD
# error "CBAUD should include the CBAUDEX bit"
#endif

speed_t
___cbaud_to_speed (tcflag_t c_cflag, speed_t other)
{
  /* The override is explicit used to support the same initialization on
     multiple platforms.  */
  DIAG_PUSH_NEEDS_COMMENT_CLANG;
  DIAG_IGNORE_NEEDS_COMMENT_CLANG (18, "-Winitializer-overrides");
  static const speed_t cbaudix_to_speed [] =
    {
      [0 ... _cbix(CBAUDMASK)] = -1,
      [_cbix(__B0)] = 0,
      [_cbix(__B50)] = 50,
      [_cbix(__B75)] = 75,
      [_cbix(__B110)] = 110,
      [_cbix(__B134)] = 134,
      [_cbix(__B150)] = 150,
      [_cbix(__B200)] = 200,
      [_cbix(__B300)] = 300,
      [_cbix(__B600)] = 600,
      [_cbix(__B1200)] = 1200,
      [_cbix(__B1800)] = 1800,
      [_cbix(__B2400)] = 2400,
      [_cbix(__B4800)] = 4800,
      [_cbix(__B9600)] = 9600,
      [_cbix(__B19200)] = 19200,
      [_cbix(__B38400)] = 38400,
      [_cbix(__B57600)] = 57600,
      [_cbix(__B115200)] = 115200,
      [_cbix(__B230400)] = 230400,
      [_cbix(__B460800)] = 460800,
      [_cbix(__B500000)] = 500000,
      [_cbix(__B576000)] = 576000,
      [_cbix(__B921600)] = 921600,
      [_cbix(__B1000000)] = 1000000,
      [_cbix(__B1152000)] = 1152000,
      [_cbix(__B1500000)] = 1500000,
      [_cbix(__B2000000)] = 2000000,
#ifdef __B7200
      [_cbix(__B7200)] = 7200,
#endif
#ifdef __B14400
      [_cbix(__B14400)] = 14400,
#endif
#ifdef __B28800
      [_cbix(__B28800)] = 28800,
#endif
#ifdef __B76800
      [_cbix(__B76800)] = 76800,
#endif
#ifdef __B153600
      [_cbix(__B153600)] = 153600,
#endif
#ifdef __B307200
      [_cbix(__B307200)] = 307200,
#endif
#ifdef __B614400
      [_cbix(__B614400)] = 614400,
#endif
#ifdef __B2500000
      [_cbix(__B2500000)] = 2500000,
#endif
#ifdef __B3000000
      [_cbix(__B3000000)] = 3000000,
#endif
#ifdef __B3500000
      [_cbix(__B3500000)] = 3500000,
#endif
#ifdef __B4000000
      [_cbix(__B4000000)] = 4000000,
#endif
    };
  DIAG_POP_NEEDS_COMMENT_CLANG;
  speed_t speed;

  if (c_cflag & (tcflag_t)(~CBAUDMASK))
    return other;

  speed = cbaudix_to_speed[_cbix(c_cflag)];
  return speed == (speed_t)-1 ? other : speed;
}

tcflag_t
___speed_to_cbaud (speed_t speed)
{
  switch (speed) {
  case 0:
    return __B0;
  case 50:
    return __B50;
  case 75:
    return __B75;
  case 110:
    return __B110;
  case 134:
    return __B134;
  case 150:
    return __B150;
  case 200:
    return __B200;
  case 300:
    return __B300;
  case 600:
    return __B600;
  case 1200:
    return __B1200;
  case 1800:
    return __B1800;
  case 2400:
    return __B2400;
  case 4800:
    return __B4800;
  case 9600:
    return __B9600;
  case 19200:
    return __B19200;
  case 38400:
    return __B38400;
  case 57600:
    return __B57600;
  case 115200:
    return __B115200;
  case 230400:
    return __B230400;
  case 460800:
    return __B460800;
  case 500000:
    return __B500000;
  case 576000:
    return __B576000;
  case 921600:
    return __B921600;
  case 1000000:
    return __B1000000;
  case 1152000:
    return __B1152000;
  case 1500000:
    return __B1500000;
  case 2000000:
    return __B2000000;
#ifdef __B76800
  case 76800:
    return __B76800;
#endif
#ifdef __B153600
  case 153600:
    return __B153600;
#endif
#ifdef __B307200
  case 307200:
    return __B307200;
#endif
#ifdef __B614400
  case 614400:
    return __B614400;
#endif
#ifdef __B2500000
  case 2500000:
    return __B2500000;
#endif
#ifdef __B3000000
  case 3000000:
    return __B3000000;
#endif
#ifdef __B3500000
  case 3500000:
    return __B3500000;
#endif
#ifdef __B4000000
  case 4000000:
    return __B4000000;
#endif
  default:
    return __BOTHER;
  }
}


/* Canonicalize the representation of speed fields in a kernel
   termios2 structure.  Specifically, if there is a valid legacy cbaud
   representation (not __BOTHER), use it and propagate the
   corresponding speed value to ispeed/ospeed, otherwise the other way
   around if possible.  Finally, if the input speed is zero, copy the
   output speed to the input speed.

   The kernel doesn't do this canonicalization, which can affect
   legacy utilities, so do it here.

   This is used by tcgetattr() and tcsetattr(). */
void
___termios2_canonicalize_speeds (struct termios2 *k_termios_p)
{
  k_termios_p->c_ospeed =
      ___cbaud_to_speed (cbaud (k_termios_p->c_cflag),  k_termios_p->c_ospeed);
  k_termios_p->c_ispeed =
      ___cbaud_to_speed (cibaud (k_termios_p->c_cflag), k_termios_p->c_ispeed);

  if (!k_termios_p->c_ispeed)
    k_termios_p->c_ispeed = k_termios_p->c_ospeed;

  k_termios_p->c_cflag &= ~(CBAUD | CIBAUD);
  k_termios_p->c_cflag |= ___speed_to_cbaud (k_termios_p->c_ospeed);
  k_termios_p->c_cflag |= ___speed_to_cbaud (k_termios_p->c_ispeed) << IBSHIFT;
}


/* Return the output baud rate stored in *TERMIOS_P.  */
speed_t
__cfgetospeed (const struct termios *termios_p)
{
  return termios_p->c_ospeed;
}
libc_hidden_def (__cfgetospeed)
versioned_symbol (libc, __cfgetospeed, cfgetospeed, GLIBC_2_42);

/* Return the input baud rate stored in *TERMIOS_P.  */
speed_t
__cfgetispeed (const struct termios *termios_p)
{
  return termios_p->c_ispeed;
}
libc_hidden_def (__cfgetispeed)
versioned_symbol (libc, __cfgetispeed, cfgetispeed, GLIBC_2_42);

/* Set the output baud rate stored in *TERMIOS_P to SPEED.  */
int
__cfsetospeed (struct termios *termios_p, speed_t speed)
{
  tcflag_t cbaud = ___speed_to_cbaud (speed);

  termios_p->c_ospeed = speed;
  termios_p->c_cflag &= ~CBAUD;
  termios_p->c_cflag |= cbaud;

  return 0;
}
libc_hidden_def (__cfsetospeed)
versioned_symbol (libc, __cfsetospeed, cfsetospeed, GLIBC_2_42);

/* Set the input baud rate stored in *TERMIOS_P to SPEED. */
int
__cfsetispeed (struct termios *termios_p, speed_t speed)
{
  tcflag_t cbaud = ___speed_to_cbaud (speed);

  termios_p->c_ispeed = speed;
  termios_p->c_cflag &= ~CIBAUD;
  termios_p->c_cflag |= cbaud << IBSHIFT;

  return 0;
}
libc_hidden_def (__cfsetispeed)
versioned_symbol (libc, __cfsetispeed, cfsetispeed, GLIBC_2_42);

#if _TERMIOS_OLD_COMPAT

/* Legacy versions which returns cbaud-encoded speed_t values */

speed_t
attribute_compat_text_section
__old_cfgetospeed (const old_termios_t *termios_p)
{
  return cbaud (termios_p->c_cflag);
}
compat_symbol (libc, __old_cfgetospeed, cfgetospeed, GLIBC_2_0);

speed_t
attribute_compat_text_section
__old_cfgetispeed (const old_termios_t *termios_p)
{
  return cibaud (termios_p->c_cflag);
}
compat_symbol (libc, __old_cfgetispeed, cfgetispeed, GLIBC_2_0);

int
attribute_compat_text_section
__old_cfsetospeed (old_termios_t *termios_p, speed_t speed)
{
  speed_t real_speed = ___cbaud_to_speed (speed, -1);
  if (real_speed == (speed_t)-1)
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (EINVAL);

#if !_HAVE_STRUCT_OLD_TERMIOS
  /* Otherwise this field doesn't exist in old_termios_t */
  termios_p->c_ospeed = real_speed;
#endif
  termios_p->c_cflag &= ~CBAUD;
  termios_p->c_cflag |= speed;

  return 0;
}
compat_symbol (libc, __old_cfsetospeed, cfsetospeed, GLIBC_2_0);

int
attribute_compat_text_section
__old_cfsetispeed (old_termios_t *termios_p, speed_t speed)
{
  speed_t real_speed = ___cbaud_to_speed (speed, -1);
  if (real_speed == (speed_t)-1)
    return INLINE_SYSCALL_ERROR_RETURN_VALUE (EINVAL);

#if !_HAVE_STRUCT_OLD_TERMIOS
  /* Otherwise this field doesn't exist in old_termios_t */
  termios_p->c_ispeed = real_speed;
#endif
  termios_p->c_cflag &= ~CIBAUD;
  termios_p->c_cflag |= speed << IBSHIFT;

  return 0;
}
compat_symbol (libc, __old_cfsetispeed, cfsetispeed, GLIBC_2_0);

#endif /* _TERMIOS_OLD_COMPAT */
