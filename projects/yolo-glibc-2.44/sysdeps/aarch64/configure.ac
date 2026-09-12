GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/aarch64.

# Static PIE is supported.
AC_DEFINE(SUPPORT_STATIC_PIE)

# We check to see if the compiler and flags are
# selecting the big endian ABI and if they are then
# we set libc_cv_aarch64_be to yes which causes
# HAVE_AARCH64_BE to be defined in config.h and
# in include/libc-symbols.h and thus available to
# shlib-versions to select the appropriate name for
# the dynamic linker via %ifdef.
AC_CACHE_CHECK([for big endian],
  [libc_cv_aarch64_be],
  [AC_EGREP_CPP(yes,[#ifdef __AARCH64EB__
                      yes
                     #endif
  ], libc_cv_aarch64_be=yes, libc_cv_aarch64_be=no)])
if test $libc_cv_aarch64_be = yes; then
  AC_DEFINE(HAVE_AARCH64_BE)
  LIBC_CONFIG_VAR([default-abi], [lp64_be])
else
  LIBC_CONFIG_VAR([default-abi], [lp64])
fi

if test x"$build_mathvec" = xnotset; then
  build_mathvec=yes
fi

if test $build_mathvec = no; then
  AC_MSG_WARN([mathvec is disabled, this results in incomplete ABI.])
fi

libc_cv_support_sframe=yes

# Check if compilers support BTI in branch protection:
LIBC_TRY_CC_AND_TEST_CC_OPTION(
  [if compiler supports -mbranch-protection=bti],
  [-Werror -mbranch-protection=bti],
  libc_cv_cc_bti,
  [libc_cv_cc_bti=yes],
  [libc_cv_cc_bti=no],
  libc_cv_test_cc_bti,
  [libc_cv_test_cc_bti=yes],
  [libc_cv_test_cc_bti=no]
)

# Check if linker supports BTI marking
LIBC_LINKER_FEATURE(
  [-z force-bti], [-Wl,-z,force-bti],
  [libc_cv_ld_bti=yes], [libc_cv_ld_bti=no]
)

# Check if toolchain is enabled to support BTI
LIBC_CHECK_ELF_PROPERTY(
  [if toolchain is enabled to support BTI],
  [AArch64 feature: .*BTI],
  [libc_cv_bti_supported=yes],
  [libc_cv_bti_supported=no]
)

if test $libc_cv_cc_bti = yes && \
  test $libc_cv_test_cc_bti = yes && \
  test $libc_cv_ld_bti = yes && \
  test $libc_cv_bti_supported = yes
then
  LIBC_CONFIG_VAR([have-test-bti], [yes])
else
  LIBC_CONFIG_VAR([have-test-bti], [no])
fi

# Check if compilers support GCS in branch protection:
LIBC_TRY_CC_AND_TEST_CC_OPTION(
  [if compiler supports -mbranch-protection=gcs],
  [-Werror -mbranch-protection=gcs],
  libc_cv_cc_gcs,
  [libc_cv_cc_gcs=yes],
  [libc_cv_cc_gcs=no],
  libc_cv_test_cc_gcs,
  [libc_cv_test_cc_gcs=yes],
  [libc_cv_test_cc_gcs=no]
)

# Check if linker supports GCS marking
LIBC_LINKER_FEATURE(
  [-z gcs=always], [-Wl,-z,gcs=always],
  [libc_cv_ld_gcs=yes], [libc_cv_ld_gcs=no]
)

# Check if toolchain is enabled to support GCS
LIBC_CHECK_ELF_PROPERTY(
  [if toolchain is enabled to support GCS],
  [AArch64 feature: .*GCS],
  [libc_cv_gcs_supported=yes],
  [libc_cv_gcs_supported=no]
)

if test $libc_cv_cc_gcs = yes && \
  test $libc_cv_test_cc_gcs = yes && \
  test $libc_cv_ld_gcs = yes && \
  test $libc_cv_gcs_supported = yes
then
  LIBC_CONFIG_VAR([have-test-gcs], [yes])
else
  LIBC_CONFIG_VAR([have-test-gcs], [no])
fi
