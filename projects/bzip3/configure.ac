AC_PREREQ([2.68])
AC_INIT([bzip3], [m4_esyscmd(build-aux/git-version-gen .tarball-version)], [https://github.com/kspalaiologos/bzip3])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar dist-bzip2 dist-xz dist-zip color-tests])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([build-aux])

AC_MSG_CHECKING([whether system or user specified compiler flags are set])
AM_CONDITIONAL([PASSED_CFLAGS], [test -n "$CFLAGS"])
AM_COND_IF([PASSED_CFLAGS], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

AC_PROG_CC([clang gcc icc])
AC_PROG_AWK
LT_INIT

PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

AC_C_RESTRICT

AC_ARG_WITH([pthread],
			  AS_HELP_STRING([--without-pthread], [Disable use of pthread library]))
AM_CONDITIONAL([WITH_PTHREAD], [test x"$with_pthread" != xno])
AM_COND_IF([WITH_PTHREAD], [
	AC_CHECK_HEADER(pthread.h,
					[AX_PTHREAD([CFLAGS="$CFLAGS $PTHREAD_CFLAGS -DPTHREAD" LIBS="$LIBS $PTHREAD_LIBS"])],
					[AC_MSG_ERROR([pthread.h not found, use --without-pthread to skip])])
])

AC_ARG_ENABLE([arch-native],
			AS_HELP_STRING([--disable-arch-native], [Disable CPU-specific optimizations]))
AM_CONDITIONAL([ENABLE_ARCH_NATIVE], [test x"$enable_arch_native" != xno])

AC_ARG_ENABLE([static-exe],
			AS_HELP_STRING([--enable-static-exe], [Enable static builds of the executable.]))
AM_CONDITIONAL([ENABLE_STATIC], [test x"$enable_static_exe" = xyes])

AM_COND_IF([PASSED_CFLAGS], [
	AC_MSG_NOTICE([skipping compiler feature detection, using '$CFLAGS'])
], [
	AX_CHECK_COMPILE_FLAG([-O2], [CFLAGS="$CFLAGS -O2"], [])
	AX_CHECK_COMPILE_FLAG([-g3], [CFLAGS="$CFLAGS -g3"], [])
	AX_CHECK_COMPILE_FLAG([-fPIC], [CFLAGS="$CFLAGS -fPIC"], [])
	AC_MSG_NOTICE([using '$CFLAGS' plus compiler feature detection])

	AM_COND_IF([ENABLE_ARCH_NATIVE], [
				AX_CHECK_COMPILE_FLAG([-march=native], [CFLAGS="$CFLAGS -march=native"],
									[AC_MSG_ERROR([Compiler does not support native optimizations, use --disable-arch-native])])
				AX_CHECK_COMPILE_FLAG([-mtune=native], [CFLAGS="$CFLAGS -mtune=native"],
									[AC_MSG_ERROR([Compiler does not support native optimizations, use --disable-arch-native])])
	])

	AM_COND_IF([ENABLE_STATIC], [
				AX_CHECK_COMPILE_FLAG([-static], [LIBS="$LIBS -all-static"],
									[AC_MSG_ERROR([Compiler does not support static linking.])])
	])
])

AX_SUBST_TRANSFORMED_PACKAGE_NAME
AX_SUBST_MAN_DATE

AC_CONFIG_FILES([Makefile bzip3.pc])
AC_CONFIG_FILES([bzip3.1 bz3grep.1 bz3less.1 bz3more.1 bz3most.1])
AC_ARG_PROGRAM
AC_OUTPUT
