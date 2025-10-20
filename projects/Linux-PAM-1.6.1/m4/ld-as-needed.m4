#!/usr/bin/m4
dnl Check whether ld supports --as-needed

AC_DEFUN([PAM_LD_AS_NEEDED], [dnl
  AC_CACHE_CHECK([whether ld supports --as-needed],
                 [pam_cv_ld_as_needed],
                 [saved_LDFLAGS="$LDFLAGS"
                  LDFLAGS="$LDFLAGS -Wl,--as-needed"
                  AC_LINK_IFELSE([AC_LANG_PROGRAM(,)],
                                 [pam_cv_ld_as_needed=yes],
                                 [pam_cv_ld_as_needed=no])
                  LDFLAGS="$saved_LDFLAGS"])
  AS_IF([test $pam_cv_ld_as_needed = yes],
        [LDFLAGS="$LDFLAGS -Wl,--as-needed"])
])
