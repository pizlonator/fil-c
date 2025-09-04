# serial 4   -*- Autoconf -*-
# Copyright (C) 2006-2007, 2009-2024 Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# From Jim Meyering
# Provide <selinux/context.h>, if necessary.

AC_DEFUN([gl_HEADERS_SELINUX_CONTEXT_H],
[
  AC_REQUIRE([gl_LIBSELINUX])
  if test "$with_selinux" != no; then
    AC_CHECK_HEADERS([selinux/context.h],
                     [GL_GENERATE_SELINUX_CONTEXT_H=false],
                     [GL_GENERATE_SELINUX_CONTEXT_H=true])
  else
    GL_GENERATE_SELINUX_CONTEXT_H=true
  fi
])
