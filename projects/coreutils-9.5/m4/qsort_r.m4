dnl Reentrant sort function.

dnl Copyright 2014-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Paul Eggert.

AC_DEFUN([gl_FUNC_QSORT_R],
[
  dnl Persuade glibc to declare qsort_r.
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

  AC_REQUIRE([gl_STDLIB_H_DEFAULTS])

  AC_CHECK_FUNCS_ONCE([qsort_r])
  if test $ac_cv_func_qsort_r = yes; then
    AC_CACHE_CHECK([for qsort_r signature], [gl_cv_qsort_r_signature],
      [AC_LINK_IFELSE(
         [AC_LANG_PROGRAM([[#include <stdlib.h>
                            void qsort_r (void *, size_t, size_t,
                                          int (*) (void const *, void const *,
                                                   void *),
                                          void *);
                            void (*p) (void *, size_t, size_t,
                                       int (*) (void const *, void const *,
                                                void *),
                                       void *) = qsort_r;
                          ]])],
         [gl_cv_qsort_r_signature=GNU],
         [AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[#include <stdlib.h>
                               void qsort_r (void *, size_t, size_t, void *,
                                             int (*) (void *,
                                                      void const *,
                                                      void const *));
                               void (*p) (void *, size_t, size_t, void *,
                                          int (*) (void *, void const *,
                                                   void const *)) = qsort_r;
                             ]])],
            [gl_cv_qsort_r_signature=BSD],
            [gl_cv_qsort_r_signature=unknown])])])
    case $gl_cv_qsort_r_signature in
      GNU)     ;;
      BSD)     REPLACE_QSORT_R=1 ;;
      unknown) HAVE_QSORT_R=0 REPLACE_QSORT_R=1 ;;
    esac
  else
    HAVE_QSORT_R=0
  fi
])
