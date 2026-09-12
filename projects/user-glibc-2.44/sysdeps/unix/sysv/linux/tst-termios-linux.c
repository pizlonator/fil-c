/* Linux termios regression tests

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <shlib-compat.h>
#include <array_length.h>

#include <support/check.h>
#include <support/namespace.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/tty.h>

#include <k_termios.h>	       /* Definitions for the raw ioctl interface */

/* Evaluate an expression and make sure errno did not get set; return
   the value of the expression */
#define CHECKERR(expr)				\
  ({						\
    errno = 0;					\
    const __typeof (expr) _val = (expr);	\
    TEST_COMPARE(errno, 0);			\
    _val;					\
  })

/* Evaluate an expression and verify that is return a specific value,
   as well as errno not having been set. */
#define VERIFY(expr,val) TEST_COMPARE(CHECKERR(expr), val)
/* Check for zero and errno not set */
#define CHECKZERO(expr)  VERIFY(expr, 0)

/* Table of legacy speed constants */

#define BOGUS ((speed_t)-1)
#define ANY   ((speed_t)-2)

struct cbaud_table
{
  speed_t speed;
  speed_t cbaud;
  const char *name;
};

static const struct cbaud_table cbaud_table [] =
{
  { 0, __B0, "__B0" },
  { 50, __B50, "__B50" },
  { 75, __B75, "__B75" },
  { 110, __B110, "__B110" },
  { 134, __B134, "__B134" },
  { 150, __B150, "__B150" },
  { 200, __B200, "__B200" },
  { 300, __B300, "__B300" },
  { 600, __B600, "__B600" },
  { 1200, __B1200, "__B1200" },
  { 1800, __B1800, "__B1800" },
  { 2400, __B2400, "__B2400" },
  { 4800, __B4800, "__B4800" },
#ifdef __B7200
  { 7200, __B7200, "__B7200" },
#endif
  { 9600, __B9600, "__B9600" },
#ifdef __B14400
  { 14400, __B14400, "__B14400" },
#endif
  { 19200, __B19200, "__B19200" },
#ifdef __B28800
  { 28800, __B28800, "__B28800" },
#endif
  { 38400, __B38400, "__B38400" },
  { 57600, __B57600, "__B57600" },
#ifdef __B76800
  { 76800, __B76800, "__B76800" },
#endif
  { 115200, __B115200, "__B115200" },
#ifdef __B153600
  { 153600, __B153600, "__B153600" },
#endif
  { 230400, __B230400, "__B230400" },
#ifdef __B307200
  { 307200, __B307200, "__B307200" },
#endif
  { 460800, __B460800, "__B460800" },
  { 500000, __B500000, "__B500000" },
  { 576000, __B576000, "__B576000" },
#ifdef __B614400
  { 614400, __B614400, "__B614400" },
#endif
  { 921600, __B921600, "__B921600" },
  { 1000000, __B1000000, "__B1000000" },
  { 1152000, __B1152000, "__B1152000" },
  { 1500000, __B1500000, "__B1500000" },
  { 2000000, __B2000000, "__B2000000" },
#ifdef __B2500000
  { 2500000, __B2500000, "__B2500000" },
#endif
#ifdef __B3000000
  { 3000000, __B3000000, "__B3000000" },
#endif
#ifdef __B3500000
  { 3500000, __B3500000, "__B3500000" },
#endif
#ifdef __B4000000
  { 4000000, __B4000000, "__B4000000" },
#endif
  { ANY, __BOTHER, "__BOTHER" },
  { BOGUS, BOGUS, "invalid" }
};

/* List of common speeds to test */

static const speed_t test_speeds [] =
{
  0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400,
  4800, 7200, 9600, 14400, 19200, 28800, 33600, 38400, 57600,
  76800, 115200, 153600, 230400, 307200, 460800, 500000,
  576000, 614400, 921600, 1000000, 1152000, 1500000, 2000000,
  2500000, 3000000, 3500000, 4000000, 5000000, 10000000
};

/* Speed function tests */

/* These intentionally are a separate implementation from speed.c;
   these should be "trivially correct" and don't need to be optimized
   in any way */

/* Returns __BOTHER if there is no legacy value for this speed */
static speed_t speed_to_cbaud (speed_t speed)
{
  const struct cbaud_table *ct;
  for (ct = cbaud_table; ct->speed != ANY; ct++)
    {
      if (ct->speed == speed)
	break;
    }
  return ct->cbaud;
}

static const char *cbaud_name (speed_t cbaud)
{
  const struct cbaud_table *ct;
  for (ct = cbaud_table; ct->cbaud != BOGUS; ct++)
    {
      if (ct->cbaud == cbaud)
	break;
    }
  return ct->name;
}

static int check_speed (speed_t expected, speed_t speed, speed_t cbaud,
			speed_t cfspeed, baud_t cfbaud, char io)
{
  speed_t want_cbaud;
  cbaud &= CBAUD;

  if (expected != ANY && speed != expected)
    FAIL_RET ("c_%cspeed = %u, expected %u", io, speed, expected);

  if (cfspeed != speed)
    FAIL_RET ("cfget%cspeed = %u, expected %u", io, cfspeed, speed);

  if (cfbaud != cfspeed)
    FAIL_RET ("cfget%cbaud = %u, but cfget%cspeed = %u",
	      io, cfbaud, io, cfspeed);

  want_cbaud = speed_to_cbaud (speed);

  if (cbaud != want_cbaud)
    FAIL_RET ("c_%cspeed = %u: %s = %s (%06o), should be %s (%06o)",
	      io, speed,
	      io == 'o' ? "CBAUD" : "CIBAUD", cbaud_name (cbaud), cbaud,
	      cbaud_name (want_cbaud), want_cbaud);

  return 0;
}

/* Validate that the speeds in the struct termios are properly normalized.
   The difference is the handling of ispeed == 0. */

/* Use this after cfset* () */
static void check_speeds_cf (const struct termios *tio_p,
			     speed_t ospeed, speed_t ispeed)
{
  check_speed (ospeed, tio_p->c_ospeed, tio_p->c_cflag,
	       CHECKERR (cfgetospeed (tio_p)),
	       CHECKERR (cfgetobaud (tio_p)), 'o');
  check_speed (ispeed, tio_p->c_ispeed, tio_p->c_cflag >> IBSHIFT,
	       CHECKERR (cfgetispeed (tio_p)),
	       CHECKERR (cfgetibaud (tio_p)), 'i');
}

/* Access the raw kernel interface and verify that the result is
   canonicalized properly; this should be run after tcsetattr (). */
static void
check_speeds_kernel (int fd, speed_t ospeed, speed_t ispeed)
{
  struct termios2 k_termios;
  tcflag_t expect_cbaud = speed_to_cbaud (ospeed);
  tcflag_t expect_cibaud;

  if (!ispeed)
    ispeed = ospeed;

  /* If ospeed == ispeed, tcsetattr() should set the kernel CIBAUD to 0,
     for compatibility with programs that use the direct ioctl interface
     but fail to account for CIBAUD. c_ispeed should still be correct. */
  if (ospeed == ispeed)
    expect_cibaud = 0;
  else
    expect_cibaud = speed_to_cbaud (ispeed);

  memset (&k_termios, 0xed, sizeof k_termios); /* Fill with nonsense */
  CHECKZERO (ioctl(fd, TCGETS2, &k_termios));

  tcflag_t k_cbaud  = k_termios.c_cflag & CBAUD;
  tcflag_t k_cibaud = (k_termios.c_cflag >> IBSHIFT) & CBAUD;

  if (k_termios.c_ospeed != ospeed)
    FAIL ("opeed %u ispeed %u: kernel c_ospeed = %u, expected %u",
	  ospeed, ispeed,
	  k_termios.c_ospeed, ospeed);

  if (k_cbaud != expect_cbaud)
    FAIL ("ospeed %u ispeed %u: kernel CBAUD = %s (%06o), expected %s (%06o)",
	  ospeed, ispeed,
	  cbaud_name (k_cbaud), k_cbaud,
	  cbaud_name (expect_cbaud), expect_cbaud);

  if (k_termios.c_ispeed != ispeed)
    FAIL ("ospeed %u ispeed %u: kernel c_ispeed == %u, expected %u",
	  ospeed, ispeed,
	  k_termios.c_ispeed, ispeed);

  if (k_cibaud != expect_cibaud)
    FAIL ("ospeed %u ispeed %u: kernel CIBAUD = %s (%06o), expected %s (%06o)",
	  ospeed, ispeed,
	  cbaud_name (k_cibaud), k_cibaud,
	  cbaud_name (expect_cibaud), expect_cibaud);
}

/* Use this after tcsetattr () */
static void check_speeds_tc (int fd, speed_t ospeed, speed_t ispeed)
{
  struct termios tio;

  if (!ispeed)
    ispeed = ospeed;

  CHECKZERO (tcgetattr (fd, &tio));
  check_speeds_cf (&tio, ospeed, ispeed);
  check_speeds_kernel (fd, ospeed, ispeed);
}

/* For search and replace convenience */
#define check_bauds_cf check_speeds_cf
#define check_bauds_tc check_speeds_tc

/* Common routine for setting speeds, with checking */
static void
set_speeds (int fd, speed_t ospeed, speed_t ispeed)
{
  struct termios tio;

  CHECKZERO (tcgetattr (fd, &tio));
  CHECKZERO (cfsetospeed (&tio, ospeed));
  CHECKZERO (cfsetispeed (&tio, ispeed));
  check_speeds_cf (&tio, ospeed, ispeed);
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_speeds_tc (fd, ospeed, ispeed);
}

/* Actual tests */

typedef void (*speed_test_t)(int ttyfd, speed_t speed);
static void
run_speed_test (int fd, speed_test_t test);

/* New interface cfset*speed test */
static void
new_cfspeed_test (int fd, speed_t speed)
{
  struct termios tio;
  speed_t old_ospeed, old_ispeed;

  CHECKZERO (tcgetattr (fd, &tio));
  old_ospeed = CHECKERR (cfgetospeed (&tio));
  old_ispeed = CHECKERR (cfgetispeed (&tio));

  /* Check initial normalization */
  check_speeds_cf (&tio, old_ospeed, old_ispeed);

  /* Check cfset*speed normalization */
  CHECKZERO (cfsetospeed (&tio, speed));
  check_speeds_cf (&tio, speed, old_ispeed);
  CHECKZERO (cfsetispeed (&tio, speed));
  check_speeds_cf (&tio, speed, speed);
  CHECKZERO (cfsetospeed (&tio, old_ospeed));
  check_speeds_cf (&tio, old_ospeed, speed);
  CHECKZERO (cfsetispeed (&tio, B0));
  check_speeds_cf (&tio, old_ospeed, B0);
  CHECKZERO (cfsetspeed (&tio, speed));
  check_speeds_cf (&tio, speed, speed);
  CHECKZERO (cfsetospeed (&tio, old_ospeed));
  CHECKZERO (cfsetispeed (&tio, old_ispeed));
  check_speeds_cf (&tio, old_ospeed, old_ispeed);
}

/* New interface cfset*speed test with tcsetattr */
static void
new_tcspeed_test (int fd, speed_t speed)
{
  struct termios tio;
  speed_t old_ospeed, old_ispeed;

  CHECKZERO (tcgetattr (fd, &tio));
  old_ospeed = CHECKERR (cfgetospeed (&tio));
  old_ispeed = CHECKERR (cfgetispeed (&tio));

  /* Check initial normalization */
  check_speeds_cf (&tio, old_ospeed, old_ispeed);

  /* Check cfset*speed normalization */
  CHECKZERO (cfsetospeed (&tio, speed));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_speeds_tc (fd, speed, old_ispeed);
  CHECKZERO (cfsetispeed (&tio, speed));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_speeds_tc (fd, speed, speed);
  CHECKZERO (cfsetospeed (&tio, old_ospeed));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_speeds_tc (fd, old_ospeed, speed);
  CHECKZERO (cfsetispeed (&tio, B0));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_speeds_tc (fd, old_ospeed, B0);
  CHECKZERO (cfsetspeed (&tio, speed));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_speeds_tc (fd, speed, speed);
  CHECKZERO (cfsetospeed (&tio, old_ospeed));
  CHECKZERO (cfsetispeed (&tio, old_ispeed));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_speeds_tc (fd, old_ospeed, old_ispeed);
}

/* New interface cfset*baud test */
static void
new_cfbaud_test (int fd, baud_t baud)
{
  struct termios tio;
  baud_t old_obaud, old_ibaud;

  CHECKZERO (tcgetattr (fd, &tio));
  old_obaud = CHECKERR (cfgetobaud (&tio));
  old_ibaud = CHECKERR (cfgetibaud (&tio));

  /* Check initial normalization */
  check_bauds_cf (&tio, old_obaud, old_ibaud);

  /* Check cfset*baud normalization */
  CHECKZERO (cfsetobaud (&tio, baud));
  check_bauds_cf (&tio, baud, old_ibaud);
  CHECKZERO (cfsetibaud (&tio, baud));
  check_bauds_cf (&tio, baud, baud);
  CHECKZERO (cfsetobaud (&tio, old_obaud));
  check_bauds_cf (&tio, old_obaud, baud);
  CHECKZERO (cfsetibaud (&tio, B0));
  check_bauds_cf (&tio, old_obaud, B0);
  CHECKZERO (cfsetbaud (&tio, baud));
  check_bauds_cf (&tio, baud, baud);
  CHECKZERO (cfsetobaud (&tio, old_obaud));
  CHECKZERO (cfsetibaud (&tio, old_ibaud));
  check_bauds_cf (&tio, old_obaud, old_ibaud);
}

/* New interface cfset*baud test with tcsetattr */
static void
new_tcbaud_test (int fd, baud_t baud)
{
  struct termios tio;
  baud_t old_obaud, old_ibaud;

  CHECKZERO (tcgetattr (fd, &tio));
  old_obaud = CHECKERR (cfgetobaud (&tio));
  old_ibaud = CHECKERR (cfgetibaud (&tio));

  /* Check initial normalization */
  check_bauds_cf (&tio, old_obaud, old_ibaud);

  /* Check cfset*baud normalization */
  CHECKZERO (cfsetobaud (&tio, baud));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_bauds_tc (fd, baud, old_ibaud);
  CHECKZERO (cfsetibaud (&tio, baud));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_bauds_tc (fd, baud, baud);
  CHECKZERO (cfsetobaud (&tio, old_obaud));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_bauds_tc (fd, old_obaud, baud);
  CHECKZERO (cfsetibaud (&tio, B0));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_bauds_tc (fd, old_obaud, B0);
  CHECKZERO (cfsetbaud (&tio, baud));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_bauds_tc (fd, baud, baud);
  CHECKZERO (cfsetobaud (&tio, old_obaud));
  CHECKZERO (cfsetibaud (&tio, old_ibaud));
  CHECKZERO (tcsetattr (fd, TCSANOW, &tio));
  check_bauds_tc (fd, old_obaud, old_ibaud);
}

/*
 * Old interface tests. This depends critically on the new struct
 * termios being guaranteed to be a superset of the legacy struct
 * termios.
 */
#if TEST_COMPAT (libc, GLIBC_2_0, GLIBC_2_42)
extern int __old_cfsetospeed (struct termios *tio_p, speed_t speed);
compat_symbol_reference (libc, __old_cfsetospeed, cfsetospeed, GLIBC_2_0);
extern int __old_cfsetispeed (struct termios *tio_p, speed_t speed);
compat_symbol_reference (libc, __old_cfsetispeed, cfsetispeed, GLIBC_2_0);
extern speed_t __old_cfgetospeed (const struct termios *tio_p);
compat_symbol_reference (libc, __old_cfgetospeed, cfgetospeed, GLIBC_2_0);
extern speed_t __old_cfgetispeed (const struct termios *tio_p);
compat_symbol_reference (libc, __old_cfgetispeed, cfgetispeed, GLIBC_2_0);
extern int __old_tcsetattr (int fd, int act, const struct termios *tio_p);
compat_symbol_reference (libc, __old_tcsetattr, tcsetattr, GLIBC_2_0);
extern int __old_tcgetattr (int fd, struct termios *tio_p);
compat_symbol_reference (libc, __old_tcgetattr, tcgetattr, GLIBC_2_0);

static int old_tcsetattr (int fd, const struct termios *tio_p)
{
  struct termios old_tio = *tio_p;

  /* Deliberately corrupt c_ispeed and c_ospeed */
  old_tio.c_ispeed = 0xdeadbeef;
  old_tio.c_ospeed = 0xfeedface;
  return __old_tcsetattr (fd, TCSANOW, &old_tio);
}
static int old_tcgetattr (int fd, struct termios *tio_p)
{
  int rv;
  memset (tio_p, 0xde, sizeof *tio_p);
  rv = __old_tcgetattr (fd, tio_p);
  if (rv)
    return rv;

  /* Deliberately corrupt c_ispeed and c_ospeed */
  tio_p->c_ispeed = 0xdeadbeef;
  tio_p->c_ospeed = 0xfeedface;
  return 0;
}

/* Old interface test. This relies on the new struct termios always
   being a binary superset of the old one.
   This doesn't bother testing split speed, since that never worked
   on the old glibc. */
static void
old_tcspeed_test (int fd, speed_t speed)
{
  struct termios tio;
  speed_t cbaud;

  if (!speed)
    return;			/* Skip B0 for this test */

  cbaud = speed_to_cbaud (speed);
  if (cbaud == __BOTHER)
    return;

  CHECKZERO (old_tcgetattr (fd, &tio));
  CHECKZERO (__old_cfsetospeed (&tio, cbaud));
  VERIFY (__old_cfgetospeed (&tio), cbaud);
  CHECKZERO (__old_cfsetispeed (&tio, cbaud));
  VERIFY (__old_cfgetispeed (&tio), cbaud);
  CHECKZERO (old_tcsetattr (fd, &tio));
  check_speeds_tc (fd, speed, speed);
}

/* Returns ANY if cbaud is __BOTHER, or BOGUS if invalid */
static speed_t cbaud_to_speed (speed_t cbaud)
{
  const struct cbaud_table *ct;
  for (ct = cbaud_table; ct->cbaud != BOGUS; ct++)
    {
      if (ct->cbaud == cbaud)
	break;
    }
  return ct->speed;
}

/* Verify that invalid CBAUD values return error for the old interfaces */
static void
old_invalid_speeds_test (int fd)
{
  struct termios tio;
  speed_t cbaud;

  for (cbaud = 0 ; cbaud ; cbaud > 0xffff ? (cbaud <<= 1) : cbaud++) {
    speed_t realspeed;
    realspeed = (cbaud & ~CBAUD) ? BOGUS : cbaud_to_speed (cbaud);
    if (realspeed >= ANY)
      {
	int rv;

	errno = 0;
	rv = __old_cfsetospeed (&tio, cbaud);
	if (rv != -1 || errno != EINVAL)
	  FAIL("__old_cfsetospeed() accepted invalid value %06o", cbaud);

	errno = 0;
	rv = __old_cfsetispeed (&tio, cbaud);
	if (rv != -1 || errno != EINVAL)
	  FAIL("__old_cfsetispeed() accepted invalid value %06o", cbaud);
      }
    else
      {
	CHECKZERO (__old_cfsetospeed (&tio, cbaud));
	VERIFY (__old_cfgetospeed (&tio), cbaud);
	CHECKZERO (__old_cfsetispeed (&tio, cbaud));
	VERIFY (__old_cfgetispeed (&tio), cbaud);
	if (cbaud)
	  {
	    CHECKZERO (old_tcsetattr (fd, &tio));
	    check_speeds_tc (fd, realspeed, realspeed);
	  }
      }
  }
}

static void
compat_tests (int fd)
{
  run_speed_test (fd, old_tcspeed_test);
  old_invalid_speeds_test (fd);
}
#else /* No TEST_COMPAT */
#define compat_tests(fd) ((void)(fd))
#endif

static void
run_speed_test (int fd, speed_test_t test)
{
  unsigned short seed [3] = { 0x1234, 0x5678, 0x9abc };
  struct speeds {
    speed_t ospeed, ispeed;
  };
  static const struct speeds initial_speeds [] = {
    { 2400, 2400 },		/* Standard speed, non-split */
    { 123456, 123456 },		/* Nonstandard speed, non-split */
    { 75, 1200 },		/* Standard split speeds */
    { 9600, 456789 },		/* One standard, one nonstandard */
    { 54321, 1234567890 }	/* Nonstandard, one very high */
  };

  array_foreach_const (is, initial_speeds)
    {
      /* Set up initial conditions */
      set_speeds (fd, is->ospeed, is->ispeed);

      /* Test all common speeds */
      array_foreach_const (ts, test_speeds)
	test (fd, *ts);

      /* Test pseudorandom speeds; array_length(test_speeds)
	 here is an arbitrary value */
      const size_t random_test_count = array_length(test_speeds);
      for (size_t i = 0 ; i < random_test_count ; i++)
	test (fd, (speed_t) jrand48 (seed));

      /* Test power-of-2 speeds */
      for (speed_t s = 1 ; s ; s <<= 1)
	test (fd, s);

      /* Test power of 2 multiples of 75; 75 << 25 is the maximum below 2^32 */
      for (int i = 0 ; i < 26 ; i++)
	test (fd, (speed_t)75 << i);
    }
}

static void
run_speed_tests (int fd)
{
  /* Test proper canonicalization using the new interface */
  run_speed_test (fd, new_cfspeed_test);
  run_speed_test (fd, new_tcspeed_test);

  /* Try the new cfset*baud() functions */
  run_speed_test (fd, new_cfbaud_test);
  run_speed_test (fd, new_tcbaud_test);

  /* Tests of the legacy functions */
  compat_tests (fd);
}

/* test dispatch */

static void
run_in_chroot (void)
{
  /* Create a pty slave to use as a tty. Most of the termios settings,
     including the speeds, have no impact on a pty, but they are still
     settable like for any other tty, which makes them very convenient
     for testing. */
  int ptmfd, ttyfd;

  support_openpty (&ptmfd, &ttyfd, NULL, NULL, NULL);
  run_speed_tests (ttyfd);
  close (ttyfd);
  close (ptmfd);
}

static int
do_test (void)
{
  support_become_root ();
  run_in_chroot ();

  return 0;
}

#include <support/test-driver.c>
