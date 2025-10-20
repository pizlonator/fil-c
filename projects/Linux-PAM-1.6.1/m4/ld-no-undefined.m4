#!/usr/bin/m4
dnl Check whether ld supports --no-undefined

AC_DEFUN([PAM_LD_NO_UNDEFINED], [dnl
  AC_CACHE_CHECK([whether ld supports --no-undefined],
                 [pam_cv_ld_no_undefined],
                 [saved_LDFLAGS="$LDFLAGS"
                  LDFLAGS="$LDFLAGS -Wl,--no-undefined"
                  AC_LINK_IFELSE([AC_LANG_PROGRAM(,)],
                                 [pam_cv_ld_no_undefined=yes],
                                 [pam_cv_ld_no_undefined=no])
                  LDFLAGS="$saved_LDFLAGS"])
  AS_IF([test $pam_cv_ld_no_undefined = yes],
        [LDFLAGS="$LDFLAGS -Wl,--no-undefined"])
])
