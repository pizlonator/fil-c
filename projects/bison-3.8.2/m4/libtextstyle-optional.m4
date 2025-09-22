# libtextstyle-optional.m4 serial 2
dnl Copyright (C) 2019-2021 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl gl_LIBTEXTSTYLE_OPTIONAL([MINIMUM-VERSION])
dnl Searches for an installed libtextstyle with version >= MINIMUM-VERSION
dnl   MINIMUM-VERSION = 0.20      - the first release in 2019
dnl   MINIMUM-VERSION = 0.20.5    - adds hyperlink support and ostream_printf
dnl   MINIMUM-VERSION unspecified - the newest release
dnl or uses the included source code parts that define only part of the API
dnl and do not do any styling.
dnl If found, it sets and AC_SUBSTs HAVE_LIBTEXTSTYLE=yes and the LIBTEXTSTYLE
dnl and LTLIBTEXTSTYLE variables, and augments the CPPFLAGS variable, and
dnl #defines HAVE_LIBTEXTSTYLE to 1.
dnl Otherwise, it sets and AC_SUBSTs HAVE_LIBTEXTSTYLE=no and LIBTEXTSTYLE and
dnl LTLIBTEXTSTYLE to empty.

AC_DEFUN([gl_LIBTEXTSTYLE_OPTIONAL],
[
  gl_LIBTEXTSTYLE([$1])
  if test $HAVE_LIBTEXTSTYLE = yes; then
    TEXTSTYLE_H=
  else
    TEXTSTYLE_H=textstyle.h
    AC_REQUIRE([AC_C_INLINE])
    AC_CHECK_FUNCS_ONCE([tcdrain])
  fi
  AC_SUBST([TEXTSTYLE_H])
  AM_CONDITIONAL([GL_GENERATE_TEXTSTYLE_H], [test -n "$TEXTSTYLE_H"])
])
