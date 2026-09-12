GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/arc.

libc_cv_have_sdata_section=no

# For ARC, historically ; was used for comments and not newline
# Later # also got added to comment list, but ; couldn't be switched to
# canonical newline as there's lots of code out there which will break
libc_cv_asm_line_sep='`'
AC_DEFINE_UNQUOTED(ASM_LINE_SEP, $libc_cv_asm_line_sep)

# ARC big endian ABI is no longer supported.
AC_CACHE_CHECK([for big endian],
  [libc_cv_arc_be],
  [AC_EGREP_CPP(yes,[#ifdef __BIG_ENDIAN__
                      yes
                     #endif
  ], libc_cv_arc_be=yes, libc_cv_arc_be=no)])
if test $libc_cv_arc_be = yes; then
  AC_MSG_ERROR([big-endian not supported])
fi
LIBC_CONFIG_VAR([default-abi], [arcle])
