# atomic-cas.m4 serial 1
dnl Copyright (C) 2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Determines whether the atomic compare-and-swap operations, officially
# supported in GCC >= 4.1 and clang >= 3.0, are actually available.
# These primitives are documented in
# <https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html>
# without platform restrictions. But they are not actually available
# everywhere:
#   * On some platforms, __sync_bool_compare_and_swap does not expand
#     to standalone inline code, but to a call to a function
#     '__sync_bool_compare_and_swap_4' (assuming a 32-bit value).
#     This is the case on
#       - arm,                                    for all gcc versions
#       - hppa, hppa64,                           for all gcc versions
#       - i386 (without '-march=i486'),           for all gcc versions
#       - sparc (32-bit, for certain CPU models), for all gcc versions
#       - m68k,                                   for gcc < 4.7
#       - mips,                                   for gcc < 4.3
#     This can be seen by compiling this piece of code
#     ----------------------------------------------------------------
#     int cmpxchg (int* value, int comp_val, int new_val)
#     {
#       return __sync_val_compare_and_swap (value, comp_val, new_val);
#     }
#     ----------------------------------------------------------------
#     with option -S, using a (native or cross-) compiler.
#   * The function __sync_bool_compare_and_swap_4 is meant to be included
#     in libgcc. And indeed, libgcc contains the source code for this
#     function on
#       - arm,          for gcc versions >= 4.7, but only for Linux
#                       and (for gcc >= 5) FreeBSD,
#       - hppa, hppa64, for gcc versions >= 4.7, but only for Linux
#                       and (for gcc >= 13) NetBSD, OpenBSD, hppa64 HP-UX
#       - i386,         never at all
#       - sparc,        never at all
#       - m68k,         for gcc versions >= 4.7, but only for Linux
#       - mips,         never at all
#   * The NetBSD C library provides this function on
#       - arm, arm64,
#       - i386,
#       - sparc,
#       - m68k,
#       - riscv64.
#     Other C libraries (e.g. glibc, musl libc) do not provide this function.
# So, the use of these primitives results in a link error on:
#   - arm, with gcc versions >= 4.1, < 4.7 on all systems except NetBSD,
#          with gcc versions >= 4.7, < 5 on FreeBSD, OpenBSD, Android,
#          with gcc versions >= 5 on OpenBSD, Android,
#   - hppa, hppa64, with gcc versions >= 4.1, < 4.7 on all systems,
#                   with gcc versions >= 4.7, < 13 on NetBSD, OpenBSD,
#   - i386 (without '-march=i486'), with gcc versions >= 4.1
#                                   on all systems except NetBSD,
#   - sparc (32-bit, for certain CPU models), with gcc versions >= 4.1
#                                             on all systems except NetBSD,
#   - m68k, with gcc versions >= 4.1, < 4.7 on all systems except NetBSD,
#   - mips, with gcc versions >= 4.1, < 4.3 on all systems.
# Additionally, link errors can occur if - such as on glibc systems - the libgcc
# functions are distributed through glibc, but the glibc version is older than
# the gcc version.
AC_DEFUN([gl_ATOMIC_COMPARE_AND_SWAP],
[
  AC_CACHE_CHECK([for __sync_bool_compare_and_swap],
    [gl_cv_builtin_sync_bool_compare_and_swap],
    [AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
          [[int cmpxchg (int* value, int comp_val, int new_val)
            {
              return __sync_val_compare_and_swap (value, comp_val, new_val);
            }
          ]],
          [[]])
       ],
       [gl_cv_builtin_sync_bool_compare_and_swap=yes],
       [gl_cv_builtin_sync_bool_compare_and_swap=no])
    ])
  if test $gl_cv_builtin_sync_bool_compare_and_swap = yes; then
    AC_DEFINE([HAVE_ATOMIC_COMPARE_AND_SWAP_GCC41], [1],
      [Define to 1 if the GCC 4.1 primitives for atomic compare-and-swap can be used.])
  fi
])
