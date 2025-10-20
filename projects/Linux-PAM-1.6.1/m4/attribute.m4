#!/usr/bin/m4
dnl Check for compiler attributes

AC_DEFUN([PAM_ATTRIBUTE_UNUSED], [
  AC_CACHE_CHECK([for __attribute__((unused))], [pam_cv_attribute_unused],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[int fun(int i __attribute__((unused)));]],
                       [[return fun(0);]])],
      [pam_cv_attribute_unused=yes],
      [pam_cv_attribute_unused=no])])
  AS_IF([test "$pam_cv_attribute_unused" = yes],
        [unused='__attribute__((unused))'],
        [unused=])
  AC_DEFINE_UNQUOTED([UNUSED], [$unused],
                     [Define if the compiler supports __attribute__((unused))])
])
