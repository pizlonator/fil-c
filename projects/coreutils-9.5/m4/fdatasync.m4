# fdatasync.m4 serial 9
dnl Copyright (C) 2008-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_FDATASYNC],
[
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST])

  dnl Using AC_CHECK_FUNCS_ONCE would break our subsequent AC_SEARCH_LIBS
  AC_CHECK_DECLS_ONCE([fdatasync])
  FDATASYNC_LIB=
  AC_SUBST([FDATASYNC_LIB])

  if test $ac_cv_have_decl_fdatasync = no; then
    HAVE_DECL_FDATASYNC=0
    dnl Mac OS X 10.7 has fdatasync but does not declare it.
    dnl Likewise Android with API level < 9.
    gl_CHECK_FUNCS_ANDROID([fdatasync], [[#include <unistd.h>]])
    if test $ac_cv_func_fdatasync = no; then
      HAVE_FDATASYNC=0
      case "$gl_cv_onwards_func_fdatasync" in
        future*) REPLACE_FDATASYNC=1 ;;
      esac
    fi
  else
    case "$host_os" in
      solaris*)
        dnl Solaris <= 2.6 has fdatasync() in libposix4.
        dnl Solaris 7..9 has it in librt.
        dnl Solaris 10 sometimes has it in librt <https://bugs.gnu.org/21059>.
        gl_saved_libs=$LIBS
        AC_SEARCH_LIBS([fdatasync], [rt posix4],
          [test "$ac_cv_search_fdatasync" = "none required" ||
           FDATASYNC_LIB=$ac_cv_search_fdatasync])
        LIBS=$gl_saved_libs
        ;;
      *)
        dnl Android 4.3 does not have fdatasync but declares it, and future
        dnl Android versions have it.
        gl_CHECK_FUNCS_ANDROID([fdatasync], [[#include <unistd.h>]])
        if test $ac_cv_func_fdatasync = no; then
          HAVE_FDATASYNC=0
          case "$gl_cv_onwards_func_fdatasync" in
            future*) REPLACE_FDATASYNC=1 ;;
          esac
        fi
        ;;
    esac
  fi

  dnl For backward compatibility.
  LIB_FDATASYNC="$FDATASYNC_LIB"
  AC_SUBST([LIB_FDATASYNC])
])
