# HW_HEADER_TIME_H
# ------------------
# Define HAVE_TIME_H to 1 if <time.h> is available.
AC_DEFUN([HW_HEADER_TIME_H],
[
  AC_PREREQ([2.60])dnl Older releases should work if AC_CHECK_HEADERS is used.
  AC_CHECK_HEADERS_ONCE([time.h])
])# HW_HEADER_TIME_H

# HW_LIBRT_TIMERS
# -----------------
# Set $hw_cv_librt_timers and $hw_cv_librt_timers_posix to "yes" or "no",
# respectively.  If the timer_create() function is available and
# POSIX compliant, then the system's timer_create(), timer_settime(),
# and timer_delete() functions are used. Otherwise, make sure
# the replacement functions will be built.
#
# In the case where we are cross compiling, the POSIX detection of
# the timer_create() function is skipped, and instead the usual check
# for the existence of all the timer_* functions is done using
# AC_REPLACE_FUNCS.
#
# If enable_timer_replacement=true, the replacements is forced to be built.
AC_DEFUN([HW_LIBRT_TIMERS],
[
  AC_PREREQ([2.60])dnl 2.59 should work if some AC_TYPE_* macros are replaced.
  AC_REQUIRE([HW_HEADER_TIME_H])dnl Our check evaluates HAVE_TIME_H.

  if test "xtrue" != x"$enable_timer_replacement"; then
	  AC_CHECK_FUNC([timer_create],
		[hw_cv_librt_timers=yes],
		[hw_cv_librt_timers=no])
	  AS_IF([test "$hw_cv_librt_timers" = yes],
		[AC_CACHE_CHECK([if timer_create is supported on the system],
		  [hw_cv_librt_timers_posix],
		  [AC_RUN_IFELSE(
			[AC_LANG_PROGRAM(
			  [[#if HAVE_TIME_H
			  #include <time.h>
			  #endif
              #include <errno.h>
			  static int test_timer_create()
			  {
                timer_t timerid;
                if(timer_create(CLOCK_REALTIME, NULL, &timerid) != 0)
                {
                    /* 
                      On this system, although the function is available,
                      no meaningful implementation is provided.
                    */
                    if(errno == ENOSYS)
                    {
                        return 1;
                    }
                }
                return 0;
			  }]],
			  [[return test_timer_create();]])],
			[hw_cv_librt_timers_posix=yes],
			[hw_cv_librt_timers_posix=no],
			[hw_cv_librt_timers_posix=autodetect])])],
		[hw_cv_librt_timers_posix=no])
  else
      hw_cv_librt_timers_posix=no
  fi

  # If the system does not have a POSIX timer_create(), then use
  # Check's reimplementation of the timer_* calls
  AS_IF([test "$hw_cv_librt_timers_posix" = no],
    [_HW_LIBRT_TIMERS_REPLACE])

  # If we are cross compiling, do the normal check for the
  # timer_* calls.
  AS_IF([test "$hw_cv_librt_timers_posix" = autodetect],
    [AC_REPLACE_FUNCS([timer_create timer_settime timer_delete])
     AC_CHECK_DECLS([timer_create, timer_settime, timer_delete])])
])# HW_LIBRT_TIMERS

# _HW_LIBRT_TIMERS_REPLACE
# ------------------------
# Arrange for building timer_create.c, timer_settime.c, and
# timer_delete.c.
AC_DEFUN([_HW_LIBRT_TIMERS_REPLACE],
[
  AS_IF([test "x$_hw_cv_librt_timers_replace_done" != xyes],
    [AC_LIBOBJ([timer_create])
     AC_LIBOBJ([timer_settime])
     AC_LIBOBJ([timer_delete])
    _hw_cv_librt_timers_replace_done=yes])
])# _HW_LIBRT_TIMERS_REPLACE
