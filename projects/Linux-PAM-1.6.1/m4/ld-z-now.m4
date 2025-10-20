#!/usr/bin/m4
dnl Check whether ld supports "-z now"

AC_DEFUN([PAM_LD_Z_NOW], [dnl
  AC_CACHE_CHECK([whether ld supports "-z now"],
                 [pam_cv_ld_z_now],
                 [saved_LDFLAGS="$LDFLAGS"
                  LDFLAGS="$LDFLAGS -Wl,-z,now"
                  AC_LINK_IFELSE([AC_LANG_PROGRAM(,)],
                                 [pam_cv_ld_z_now=yes],
                                 [pam_cv_ld_z_now=no])
                  LDFLAGS="$saved_LDFLAGS"])
  AS_IF([test $pam_cv_ld_z_now = yes],
        [ZNOW_LDFLAGS="-Wl,-z,now"],
        [ZNOW_LDFLAGS=])
])
