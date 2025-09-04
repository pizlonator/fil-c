# systemd.m4 serial 3
dnl Copyright (C) 2023-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Sets SYSTEMD_CHOICE to 'yes' or 'no', depending on the preferred use of
# systemd APIs.
AC_DEFUN([gl_SYSTEMD_CHOICE],
[
  AC_MSG_CHECKING([whether to use systemd APIs])
  AC_ARG_ENABLE([systemd],
    [  --enable-systemd        use systemd APIs],
    [SYSTEMD_CHOICE="$enableval"],
    [SYSTEMD_CHOICE=no])
  AC_MSG_RESULT([$SYSTEMD_CHOICE])
  AC_SUBST([SYSTEMD_CHOICE])
])

# Pre-built package name for the libsystemd library:
# - On Debian and Debian-based systems: libsystemd-dev
# - On Red Hat distributions: systemd-devel
# - Other: https://repology.org/project/systemd/versions
