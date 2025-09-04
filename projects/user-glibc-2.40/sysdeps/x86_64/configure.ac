GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/x86_64.

dnl Check if -mprefer-vector-width=128 works.
AC_CACHE_CHECK(-mprefer-vector-width=128, libc_cv_cc_mprefer_vector_width, [dnl
LIBC_TRY_CC_OPTION([-mprefer-vector-width=128],
		   [libc_cv_cc_mprefer_vector_width=yes],
		   [libc_cv_cc_mprefer_vector_width=no])
])
LIBC_CONFIG_VAR([config-cflags-mprefer-vector-width],
		[$libc_cv_cc_mprefer_vector_width])

LIBC_LINKER_FEATURE([-z mark-plt], [-Wl,-z,mark-plt],
                    [libc_cv_z_mark_plt=yes], [libc_cv_z_mark_plt=no])
LIBC_CONFIG_VAR([have-z-mark-plt], [$libc_cv_z_mark_plt])

if test x"$build_mathvec" = xnotset; then
  build_mathvec=yes
fi

if test $enable_cet != no; then
  # Check if CET can be enabled.
  AC_CACHE_CHECK(whether CET can be enabled,
		 libc_cv_x86_cet_available, [dnl
cat > conftest.c <<EOF
#if !defined __CET__ || __CET__ != 3
# error CET isn't available.
#endif
EOF
		 if AC_TRY_COMMAND(${CC-cc} -c $CFLAGS -fcf-protection -include cet.h conftest.c 1>&AS_MESSAGE_LOG_FD); then
		   libc_cv_x86_cet_available=yes
		 else
		   libc_cv_x86_cet_available=no
		 fi
		 rm -rf conftest*])
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
AC_CACHE_CHECK(whether -mamx-tile works properly,
	       libc_cv_x86_have_amx_tile, [dnl
cat > conftest.c <<EOF
#include <x86intrin.h>
EOF
	       libc_cv_x86_have_amx_tile=no
	       if AC_TRY_COMMAND(${CC-cc} -E $CFLAGS -mamx-tile conftest.c > conftest.i); then
		 if grep -q __builtin_ia32_ldtilecfg conftest.i; then
		   libc_cv_x86_have_amx_tile=yes
	         fi
	       fi
	       rm -rf conftest*])
LIBC_CONFIG_VAR([have-mamx-tile], [$libc_cv_x86_have_amx_tile])

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

test -n "$critic_missing" && AC_MSG_ERROR([
*** $critic_missing])
