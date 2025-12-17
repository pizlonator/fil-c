# pthread-rwlock.m4
# serial 4
dnl Copyright (C) 2019-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_PTHREAD_RWLOCK],
[
  AC_REQUIRE([gl_PTHREAD_H])
  AC_REQUIRE([AC_CANONICAL_HOST])

  if { case "$host_os" in mingw* | windows*) true;; *) false;; esac; } \
     && test $gl_threads_api = windows; then
    dnl Choose function names that don't conflict with the mingw-w64 winpthreads
    dnl library.
    REPLACE_PTHREAD_RWLOCK_INIT=1
    REPLACE_PTHREAD_RWLOCKATTR_INIT=1
    REPLACE_PTHREAD_RWLOCKATTR_DESTROY=1
    REPLACE_PTHREAD_RWLOCK_RDLOCK=1
    REPLACE_PTHREAD_RWLOCK_WRLOCK=1
    REPLACE_PTHREAD_RWLOCK_TRYRDLOCK=1
    REPLACE_PTHREAD_RWLOCK_TRYWRLOCK=1
    REPLACE_PTHREAD_RWLOCK_TIMEDRDLOCK=1
    REPLACE_PTHREAD_RWLOCK_TIMEDWRLOCK=1
    REPLACE_PTHREAD_RWLOCK_UNLOCK=1
    REPLACE_PTHREAD_RWLOCK_DESTROY=1
  else
    if test $HAVE_PTHREAD_H = 0; then
      HAVE_PTHREAD_RWLOCK_INIT=0
      HAVE_PTHREAD_RWLOCKATTR_INIT=0
      HAVE_PTHREAD_RWLOCKATTR_DESTROY=0
      HAVE_PTHREAD_RWLOCK_RDLOCK=0
      HAVE_PTHREAD_RWLOCK_WRLOCK=0
      HAVE_PTHREAD_RWLOCK_TRYRDLOCK=0
      HAVE_PTHREAD_RWLOCK_TRYWRLOCK=0
      HAVE_PTHREAD_RWLOCK_TIMEDRDLOCK=0
      HAVE_PTHREAD_RWLOCK_TIMEDWRLOCK=0
      HAVE_PTHREAD_RWLOCK_UNLOCK=0
      HAVE_PTHREAD_RWLOCK_DESTROY=0
    else
      dnl On Mac OS X 10.4, the pthread_rwlock_* functions exist but are not
      dnl usable because PTHREAD_RWLOCK_INITIALIZER is not defined.
      dnl On Android 4.3, the pthread_rwlock_* functions are declared in
      dnl <pthread.h> but don't exist in libc.
      AC_CACHE_CHECK([for pthread_rwlock_init],
        [gl_cv_func_pthread_rwlock_init],
        [case "$host_os" in
           darwin*)
             AC_COMPILE_IFELSE(
               [AC_LANG_SOURCE(
                  [[#include <pthread.h>
                    pthread_rwlock_t l = PTHREAD_RWLOCK_INITIALIZER;
                  ]])],
               [gl_cv_func_pthread_rwlock_init=yes],
               [gl_cv_func_pthread_rwlock_init=no])
             ;;
           *)
             saved_LIBS="$LIBS"
             LIBS="$LIBS $LIBPMULTITHREAD"
             AC_LINK_IFELSE(
               [AC_LANG_SOURCE(
                  [[extern
                    #ifdef __cplusplus
                    "C"
                    #endif
                    int pthread_rwlock_init (void);
                    int main ()
                    {
                      return pthread_rwlock_init ();
                    }
                  ]])],
               [gl_cv_func_pthread_rwlock_init=yes],
               [gl_cv_func_pthread_rwlock_init=no])
             LIBS="$saved_LIBS"
             ;;
         esac
        ])
      if test $gl_cv_func_pthread_rwlock_init = no; then
        REPLACE_PTHREAD_RWLOCK_INIT=1
        REPLACE_PTHREAD_RWLOCKATTR_INIT=1
        REPLACE_PTHREAD_RWLOCKATTR_DESTROY=1
        REPLACE_PTHREAD_RWLOCK_RDLOCK=1
        REPLACE_PTHREAD_RWLOCK_WRLOCK=1
        REPLACE_PTHREAD_RWLOCK_TRYRDLOCK=1
        REPLACE_PTHREAD_RWLOCK_TRYWRLOCK=1
        REPLACE_PTHREAD_RWLOCK_TIMEDRDLOCK=1
        REPLACE_PTHREAD_RWLOCK_TIMEDWRLOCK=1
        REPLACE_PTHREAD_RWLOCK_UNLOCK=1
        REPLACE_PTHREAD_RWLOCK_DESTROY=1
        AC_DEFINE([PTHREAD_RWLOCK_UNIMPLEMENTED], [1],
          [Define if all pthread_rwlock* functions don't exist.])
      else
        dnl On Mac OS X 10.5, FreeBSD 5.2.1, OpenBSD 3.8, AIX 5.1, HP-UX 11,
        dnl IRIX 6.5, Solaris 9, Cygwin, the pthread_rwlock_timed*lock functions
        dnl don't exist, although the other pthread_rwlock* functions exist.
        AC_CHECK_DECL([pthread_rwlock_timedrdlock], ,
          [HAVE_PTHREAD_RWLOCK_TIMEDRDLOCK=0
           HAVE_PTHREAD_RWLOCK_TIMEDWRLOCK=0
           AC_DEFINE([PTHREAD_RWLOCK_LACKS_TIMEOUT], [1],
             [Define if the functions pthread_rwlock_timedrdlock and pthread_rwlock_timedwrlock don't exist.])
          ],
          [[#include <pthread.h>]])
      fi
    fi
  fi
])
