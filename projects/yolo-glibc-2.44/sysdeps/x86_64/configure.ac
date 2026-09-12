GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/x86_64.

dnl Check if -mprefer-vector-width=128 works.
LIBC_TRY_CC_AND_TEST_CC_OPTION([for -mprefer-vector-width=128],
  [-mprefer-vector-width=128],
  libc_cv_cc_mprefer_vector_width,
  [libc_cv_cc_mprefer_vector_width=yes],
  [libc_cv_cc_mprefer_vector_width=no],
  libc_cv_test_cc_mprefer_vector_width,
  [libc_cv_test_cc_mprefer_vector_width=yes],
  [libc_cv_test_cc_mprefer_vector_width=no])
LIBC_CONFIG_VAR([config-cflags-mprefer-vector-width],
		[$libc_cv_cc_mprefer_vector_width])

LIBC_LINKER_FEATURE([-z mark-plt], [-Wl,-z,mark-plt -Wl,--fatal-warnings],
                    [libc_cv_z_mark_plt=yes], [libc_cv_z_mark_plt=no])
LIBC_CONFIG_VAR([have-z-mark-plt], [$libc_cv_z_mark_plt])

if test x"$build_mathvec" = xnotset; then
  build_mathvec=yes
fi

test_enable_cet=$enable_cet
if test $enable_cet != no; then
  # Check if CET can be enabled.
  conftest_code="
#if !defined __CET__ || __CET__ != 3
# error CET is not available.
#endif
"
  LIBC_TRY_CC_AND_TEST_CC_COMMAND([whether CET can be enabled],
    [$conftest_code],
    [-c -fcf-protection -include cet.h],
    libc_cv_x86_cet_available,
    [libc_cv_x86_cet_available=yes],
    [libc_cv_x86_cet_available=no],
    libc_cv_test_x86_cet_available,
    [libc_cv_test_x86_cet_available=yes],
    [libc_cv_test_x86_cet_available=no])
    test_enable_cet=$libc_cv_test_x86_cet_available
  if test $libc_cv_x86_cet_available != yes; then
    AC_MSG_ERROR([$CC doesn't support CET])
  fi
fi
if test $enable_cet != no; then
  # Check if assembler supports CET.
  AC_CACHE_CHECK(whether assembler supports CET,
		 libc_cv_x86_cet_as, [dnl
cat > conftest.s <<EOF
	incsspd %ecx
EOF
		 if AC_TRY_COMMAND(${CC-cc} -c $CFLAGS conftest.s -o conftest.o 1>&AS_MESSAGE_LOG_FD); then
		   libc_cv_x86_cet_as=yes
		 else
		   libc_cv_x86_cet_as=no
		 fi
		 rm -rf conftest*])
  if test $libc_cv_x86_cet_as = no; then
    AC_MSG_ERROR([$AS doesn't support CET])
  fi
fi
if test $enable_cet = yes; then
  AC_DEFINE(DEFAULT_DL_X86_CET_CONTROL, cet_elf_property)
elif test $enable_cet = permissive; then
  AC_DEFINE(DEFAULT_DL_X86_CET_CONTROL, cet_permissive)
fi
LIBC_CONFIG_VAR([enable-cet], [$enable_cet])

# Check if -mamx-tile works properly.
LIBC_TRY_TEST_CC_COMMAND([whether -mamx-tile works properly],
  [#include <x86intrin.h>],
  [-E -mamx-tile],
  libc_cv_test_x86_have_amx_tile,
  [
if test $libc_cv_test_clang = yes; then
  libc_cv_test_x86_have_amx_tile=yes
else
  if grep -q __builtin_ia32_ldtilecfg conftest; then
    libc_cv_test_x86_have_amx_tile=yes
  else
    libc_cv_test_x86_have_amx_tile=no
  fi
fi
  ],
  [libc_cv_test_x86_have_amx_tile=no])
LIBC_CONFIG_VAR([have-mamx-tile], [$libc_cv_test_x86_have_amx_tile])

# Check if -mapxf is enabled.
AC_CACHE_CHECK(whether -mapxf is enabled,
	       libc_cv_x86_have_apx, [dnl
cat > conftest.c <<EOF
#ifndef __APX_F__
# error APX isn't enabled
#endif
EOF
	       libc_cv_x86_have_apx=no
	       if AC_TRY_COMMAND(${CC-cc} -c $CFLAGS conftest.c -o conftest.o 1>&AS_MESSAGE_LOG_FD); then
		   libc_cv_x86_have_apx=yes
	       fi
	       rm -rf conftest*])
if test $libc_cv_x86_have_apx = yes; then
  AC_DEFINE(HAVE_X86_APX)
fi
LIBC_CONFIG_VAR([have-x86-apx], [$libc_cv_x86_have_apx])

libc_cv_support_sframe=yes

test -n "$critic_missing" && AC_MSG_ERROR([
*** $critic_missing])
