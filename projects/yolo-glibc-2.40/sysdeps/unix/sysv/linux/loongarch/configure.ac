sinclude(./aclocal.m4)dnl Autoconf lossage
GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/unix/sysv/linux/loongarch.

arch_minimum_kernel=5.19.0

libc_cv_loongarch_int_abi=no
AC_EGREP_CPP(4 8 8, [__SIZEOF_INT__ __SIZEOF_LONG__ __SIZEOF_POINTER__
  ], libc_cv_loongarch_int_abi=lp64)
if test $libc_cv_loongarch_int_abi = no; then
  AC_MSG_ERROR([Unable to determine integer ABI])
fi

libc_cv_loongarch_float_abi=no
AC_EGREP_CPP(yes, [#ifdef __loongarch_double_float
		   yes
		   #endif
  ],libc_cv_loongarch_float_abi=d)
AC_EGREP_CPP(yes, [#ifdef __loongarch_soft_float
		   yes
		   #endif
  ],libc_cv_loongarch_float_abi=s)
if test "$libc_cv_loongarch_float_abi" = no; then
  AC_MSG_ERROR([Unable to determine floating-point ABI])
fi

LIBC_CONFIG_VAR([default-abi], [$libc_cv_loongarch_int_abi$libc_cv_loongarch_float_abi])

case $libc_cv_loongarch_int_abi$libc_cv_loongarch_float_abi in
lp64s)
  LIBC_SLIBDIR_RTLDDIR([lib64/sf], [lib64])
  ;;
lp64d)
  LIBC_SLIBDIR_RTLDDIR([lib64], [lib64])
  ;;
esac

ldd_rewrite_script=sysdeps/unix/sysv/linux/loongarch/ldd-rewrite.sed
