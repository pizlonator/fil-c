# configmake.m4 serial 4
dnl Copyright (C) 2010-2021 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_PREREQ([2.60])

# gl_CONFIGMAKE_PREP
# ------------------
# Guarantee all of the standard directory variables, even when used with
# autoconf 2.64 (runstatedir wasn't supported before 2.70) or
# automake 1.11 (runstatedir isn't supported even in 1.16.1).
AC_DEFUN([gl_CONFIGMAKE_PREP],
[
  if test "x$lispdir" = x; then
    AC_SUBST([lispdir], ['${datarootdir}/emacs/site-lisp'])
  fi
  dnl Added in autoconf 2.70.
  if test "x$runstatedir" = x; then
    AC_SUBST([runstatedir], ['${localstatedir}/run'])
  fi

  dnl Automake 1.11 provides pkglibexecdir merely without AC_SUBST.
  dnl This blind use of AC_SUBST is safe.
  AC_SUBST([pkglibexecdir], ['${libexecdir}/${PACKAGE}'])
])
