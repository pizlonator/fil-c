#!/usr/bin/m4
dnl Check whether ld supports -O1

AC_DEFUN([PAM_LD_O1], [dnl
  AC_CACHE_CHECK([whether ld supports -O1],
                 [pam_cv_ld_O1],
                 [saved_LDFLAGS="$LDFLAGS"
                  LDFLAGS="$LDFLAGS -Wl,-O1"
                  AC_LINK_IFELSE([AC_LANG_PROGRAM(,)],
                                 [pam_cv_ld_O1=yes],
                                 [pam_cv_ld_O1=no])
                  LDFLAGS="$saved_LDFLAGS"])
  AS_IF([test $pam_cv_ld_O1 = yes],
        [LDFLAGS="$LDFLAGS -Wl,-O1"])
])
