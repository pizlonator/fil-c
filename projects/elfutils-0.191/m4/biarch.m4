AC_DEFUN([utrace_CC_m32], [dnl
AC_CACHE_CHECK([$CC option for 32-bit word size], utrace_cv_CC_m32, [dnl
save_CC="$CC"
utrace_cv_CC_m32=none
for ut_try in -m32 -m31; do
  [CC=`echo "$save_CC" | sed 's/ -m[36][241]//'`" $ut_try"]
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([[int foo (void) { return 1; }]])],
		    [utrace_cv_CC_m32=$ut_try])
  test x$utrace_cv_CC_m32 = xnone || break
done
CC="$save_CC"])])

AC_DEFUN([utrace_HOST64], [AC_REQUIRE([utrace_CC_m32])
AS_IF([test x$utrace_cv_CC_m32 != xnone], [dnl
AC_CACHE_CHECK([for 64-bit host], utrace_cv_host64, [dnl
AC_EGREP_CPP([@utrace_host64@], [#include <stdint.h>
#if (UINTPTR_MAX > 0xffffffffUL)
@utrace_host64@
#endif],
             utrace_cv_host64=yes, utrace_cv_host64=no)])
AS_IF([test $utrace_cv_host64 = no],
      [utrace_biarch=-m64 utrace_thisarch=$utrace_cv_CC_m32],
      [utrace_biarch=$utrace_cv_CC_m32 utrace_thisarch=-m64])

biarch_CC=`echo "$CC" | sed "s/ *${utrace_thisarch}//"`
biarch_CC="$biarch_CC $utrace_biarch"])])

AC_DEFUN([utrace_BIARCH], [AC_REQUIRE([utrace_HOST64])
utrace_biarch_forced=no
AC_ARG_WITH([biarch],
	    AS_HELP_STRING([--with-biarch],
			   [enable biarch tests despite build problems]),
	    [AS_IF([test "x$with_biarch" != xno], [utrace_biarch_forced=yes])])
AS_IF([test $utrace_biarch_forced = yes], [dnl
utrace_cv_cc_biarch=yes
AC_MSG_NOTICE([enabling biarch tests regardless using $biarch_CC])], [dnl
AS_IF([test x$utrace_cv_CC_m32 != xnone -a x$utrace_cv_host64 != xno], [dnl
AC_CACHE_CHECK([whether $biarch_CC makes executables we can run],
	       utrace_cv_cc_biarch, [dnl
save_CC="$CC"
CC="$biarch_CC"
AC_RUN_IFELSE([AC_LANG_PROGRAM([], [])],
	      utrace_cv_cc_biarch=yes, utrace_cv_cc_biarch=no, utrace_cv_cc_biarch=no)
CC="$save_CC"])], [utrace_cv_cc_biarch=no])
AS_IF([test $utrace_cv_cc_biarch != yes], [dnl
AC_MSG_WARN([not running biarch tests, $biarch_CC does not work])])])
AM_CONDITIONAL(BIARCH, [test $utrace_cv_cc_biarch = yes])])
