# This file is part of Autoconf.			-*- Autoconf -*-
# Macros that test for specific, unclassified, features.
#
# Copyright (C) 1992-1996, 1998-2017, 2020-2023 Free Software
# Foundation, Inc.

# This file is part of Autoconf.  This program is free
# software; you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# Under Section 7 of GPL version 3, you are granted additional
# permissions described in the Autoconf Configure Script Exception,
# version 3.0, as published by the Free Software Foundation.
#
# You should have received a copy of the GNU General Public License
# and a copy of the Autoconf Configure Script Exception along with
# this program; see the files COPYINGv3 and COPYING.EXCEPTION
# respectively.  If not, see <https://www.gnu.org/licenses/>.

# Written by David MacKenzie, with help from
# François Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
# Roland McGrath, Noah Friedman, david d zuhn, and many others.
# _AC_SYS_YEAR2038_TEST_CODE
# --------------------------
# C code used to probe for time_t that can represent time points more
# than 2**31 - 1 seconds after the epoch.  With the usual Unix epoch,
# these correspond to dates after 2038-01-18 22:14:07 +0000 (Gregorian),
# hence the name.

AC_DEFUN([_AC_SYS_YEAR2038_TEST_CODE],
[[
  #include <time.h>
  /* Check that time_t can represent 2**32 - 1 correctly.  */
  #define LARGE_TIME_T \\
    ((time_t) (((time_t) 1 << 30) - 1 + 3 * ((time_t) 1 << 30)))
  int verify_time_t_range[(LARGE_TIME_T / 65537 == 65535
                           && LARGE_TIME_T % 65537 == 0)
                          ? 1 : -1];
]])

# _AC_SYS_YEAR2038_OPTIONS
# ------------------------
# List of known ways to enable support for large time_t.  If you change
# this list you probably also need to change the AS_CASE at the end of
# _AC_SYS_YEAR2038_PROBE.
m4_define([_AC_SYS_YEAR2038_OPTIONS], m4_normalize(
    ["none needed"]                   dnl 64-bit and newer 32-bit Unix
    ["-D_TIME_BITS=64"]               dnl glibc 2.34 with some 32-bit ABIs
    ["-D__MINGW_USE_VC2005_COMPAT"]   dnl 32-bit MinGW
    ["-U_USE_32_BIT_TIME_T -D__MINGW_USE_VC2005_COMPAT"]
                                      dnl 32-bit MinGW (misconfiguration)
))

# _AC_SYS_YEAR2038_PROBE
# ----------------------
# Subroutine of AC_SYS_YEAR2038.  Probe for time_t that can represent
# time points more than 2**31 - 1 seconds after the epoch (dates after
# 2038-01-18, see above) and set the cache variable ac_cv_sys_year2038_opts
# to one of the values in the _AC_SYS_YEAR2038_OPTIONS list, or to
# "support not detected" if none of them worked.  Then, set compilation
# options and #defines as necessary to enable large time_t support.
#
# Note that we do not test whether mktime, localtime, etc. handle
# large values of time_t correctly, as that would require use of
# AC_TRY_RUN.  Note also that some systems only support large time_t
# together with large off_t.
#
# If you change this macro you may also need to change
# _AC_SYS_YEAR2038_OPTIONS.
AC_DEFUN([_AC_SYS_YEAR2038_PROBE],
[AC_CACHE_CHECK([for $CC option for timestamps after 2038],
  [ac_cv_sys_year2038_opts],
  [ac_save_CPPFLAGS="$CPPFLAGS"
  ac_opt_found=no
  for ac_opt in _AC_SYS_YEAR2038_OPTIONS; do
    AS_IF([test x"$ac_opt" != x"none needed"],
      [CPPFLAGS="$ac_save_CPPFLAGS $ac_opt"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([_AC_SYS_YEAR2038_TEST_CODE])],
      [ac_cv_sys_year2038_opts="$ac_opt"
      ac_opt_found=yes])
    test $ac_opt_found = no || break
  done
  CPPFLAGS="$ac_save_CPPFLAGS"
  test $ac_opt_found = yes || ac_cv_sys_year2038_opts="support not detected"])

ac_have_year2038=yes
AS_CASE([$ac_cv_sys_year2038_opts],
  ["none needed"], [],
  ["support not detected"],
    [ac_have_year2038=no],

  ["-D_TIME_BITS=64"],
    [AC_DEFINE([_TIME_BITS], [64],
      [Number of bits in time_t, on hosts where this is settable.])],

  ["-D__MINGW_USE_VC2005_COMPAT"],
    [AC_DEFINE([__MINGW_USE_VC2005_COMPAT], [1],
      [Define to 1 on platforms where this makes time_t a 64-bit type.])],

  ["-U_USE_32_BIT_TIME_T"*],
    [AC_MSG_FAILURE(m4_text_wrap(
      [the 'time_t' type is currently forced to be 32-bit.
       It will stop working after mid-January 2038.
       Remove _USE_32BIT_TIME_T from the compiler flags.],
      [], [], [55]))],

  [AC_MSG_ERROR(
    [internal error: bad value for \$ac_cv_sys_year2038_opts])])
])

# _AC_SYS_YEAR2038_ENABLE
# -----------------------
# Depending on which of the YEAR2038 macros was used, add either an
# --enable-year2038 or a --disable-year2038 to
# the configure script.  This is expanded very late and
# therefore there cannot be any code in the AC_ARG_ENABLE.  The
# default value for 'enable_year2038' is emitted unconditionally
# because the generated code always looks at this variable.
m4_define([_AC_SYS_YEAR2038_ENABLE],
[m4_divert_text([DEFAULTS],
  m4_provide_if([AC_SYS_YEAR2038],
    [enable_year2038=yes],
    [enable_year2038=no]))]dnl
[AC_ARG_ENABLE([year2038],
  m4_provide_if([AC_SYS_YEAR2038],
    [AS_HELP_STRING([--disable-year2038],
      [don't support timestamps after 2038])],
    [AS_HELP_STRING([--enable-year2038],
      [support timestamps after 2038])]))])

# AC_SYS_YEAR2038
# ---------------
# Attempt to detect and activate support for large time_t.
# On systems where time_t is not always 64 bits, this probe can be
# skipped by passing the --disable-year2038 option to configure.
AC_DEFUN([AC_SYS_YEAR2038],
[AC_REQUIRE([AC_SYS_LARGEFILE])dnl
AS_IF([test "$enable_year2038,$ac_have_year2038,$cross_compiling" = yes,no,no],
 [# If we're not cross compiling and 'touch' works with a large
  # timestamp, then we can presume the system supports wider time_t
  # *somehow* and we just weren't able to detect it.  One common
  # case that we deliberately *don't* probe for is a system that
  # supports both 32- and 64-bit ABIs but only the 64-bit ABI offers
  # wide time_t.  (It would be inappropriate for us to override an
  # intentional use of -m32.)  Error out, demanding use of
  # --disable-year2038 if this is intentional.
  AS_IF([TZ=UTC0 touch -t 210602070628.15 conftest.time 2>/dev/null],
    [AS_CASE([`TZ=UTC0 LC_ALL=C ls -l conftest.time 2>/dev/null`],
       [*'Feb  7  2106'* | *'Feb  7 17:10'*],
       [AC_MSG_FAILURE(m4_text_wrap(
	  [this system appears to support timestamps after mid-January 2038,
	   but no mechanism for enabling wide 'time_t' was detected.
	   Did you mean to build a 64-bit binary? (E.g., 'CC="${CC} -m64"'.)
	   To proceed with 32-bit time_t, configure with '--disable-year2038'.],
	  [], [], [55]))])])])])

# AC_SYS_YEAR2038_RECOMMENDED
# ---------------------------
# Same as AC_SYS_YEAR2038, but recommend support for large time_t.
# If we cannot find any way to make time_t capable of representing
# values larger than 2**31 - 1, error out unless --disable-year2038 is given.
AC_DEFUN([AC_SYS_YEAR2038_RECOMMENDED],
[AC_REQUIRE([AC_SYS_YEAR2038])dnl
AS_IF([test "$enable_year2038,$ac_have_year2038" = yes,no],
   [AC_MSG_FAILURE(m4_text_wrap(
      [could not enable timestamps after mid-January 2038.
       This package recommends support for these later timestamps.
       However, to proceed with signed 32-bit time_t even though it
       will fail then, configure with '--disable-year2038'.],
      [], [], [55]))])])

AC_DEFUN([UL_YEAR2038_INIT],[
  AS_IF([test "$enable_year2038" != no],
    [_AC_SYS_YEAR2038_PROBE])
  AC_CONFIG_COMMANDS_PRE([_AC_SYS_YEAR2038_ENABLE])])
