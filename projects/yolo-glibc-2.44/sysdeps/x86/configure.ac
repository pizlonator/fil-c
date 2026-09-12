GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/x86.

# Check if linker supports x86 ISA level.
AC_CACHE_CHECK([for linker x86 ISA level support],
	       libc_cv_include_x86_isa_level, [dnl
cat > conftest1.S <<EOF
#ifdef __LP64__
# define P2ALIGN 3
#else
# define P2ALIGN 2
#endif
	.section ".note.gnu.property", "a"
	.p2align P2ALIGN
	.long 1f - 0f		/* name length.  */
	.long 4f - 1f		/* data length.  */
	/* NT_GNU_PROPERTY_TYPE_0 */
	.long 5			/* note type.  */
0:
	.asciz "GNU"		/* vendor name.  */
1:
	.p2align P2ALIGN
	/* GNU_PROPERTY_X86_ISA_1_NEEDED */
	.long 0xc0008002	/* pr_type.  */
	.long 3f - 2f		/* pr_datasz.  */
2:
	.long 0x1
3:
	.p2align P2ALIGN
4:
EOF
cat > conftest2.S <<EOF
#ifdef __LP64__
# define P2ALIGN 3
#else
# define P2ALIGN 2
#endif
	.section ".note.gnu.property", "a"
	.p2align P2ALIGN
	.long 1f - 0f		/* name length.  */
	.long 4f - 1f		/* data length.  */
	/* NT_GNU_PROPERTY_TYPE_0 */
	.long 5			/* note type.  */
0:
	.asciz "GNU"		/* vendor name.  */
1:
	.p2align P2ALIGN
	/* GNU_PROPERTY_X86_ISA_1_NEEDED */
	.long 0xc0008002	/* pr_type.  */
	.long 3f - 2f		/* pr_datasz.  */
2:
	.long 0x2
3:
	.p2align P2ALIGN
4:
EOF
libc_cv_include_x86_isa_level=no
if AC_TRY_COMMAND(${CC-cc} $CFLAGS $CPPFLAGS -nostartfiles -nostdlib -r -o conftest conftest1.S conftest2.S); then
  count=`LC_ALL=C $READELF -n conftest | grep NT_GNU_PROPERTY_TYPE_0 | wc -l`
  if test "$count" -eq 1; then
    libc_cv_include_x86_isa_level=yes
  fi
fi
rm -f conftest*])
if test $libc_cv_include_x86_isa_level = yes; then
  AC_DEFINE(INCLUDE_X86_ISA_LEVEL)
  AC_CACHE_CHECK([for LAHF/SAHF instruction support],
		 libc_cv_have_x86_lahf_sahf, [dnl
    libc_cv_have_x86_lahf_sahf=no
    if AC_TRY_COMMAND(${CC-cc} $CFLAGS $CPPFLAGS -fverbose-asm -S -o - -x c /dev/null) | grep -qE '(-msahf\b|-march=x86-64-v)'; then
      libc_cv_have_x86_lahf_sahf=yes
    fi])
  if test $libc_cv_have_x86_lahf_sahf = yes; then
    AC_DEFINE(HAVE_X86_LAHF_SAHF)
    ISAFLAG="-DHAVE_X86_LAHF_SAHF"
  fi
  AC_CACHE_CHECK([for MOVBE instruction support],
		 libc_cv_have_x86_movbe, [dnl
    libc_cv_have_x86_movbe=no
    if AC_TRY_COMMAND(${CC-cc} $CFLAGS $CPPFLAGS -fverbose-asm -S -o - -x c /dev/null) | grep -qE '(-mmovbe\b|-march=x86-64-v(@<:@3-9@:>@|@<:@1-9@:>@@<:@0-9@:>@))'; then
      libc_cv_have_x86_movbe=yes
    fi])
  if test $libc_cv_have_x86_movbe = yes; then
    AC_DEFINE(HAVE_X86_MOVBE)
    ISAFLAG="$ISAFLAG -DHAVE_X86_MOVBE"
  fi

  # Check for ISA level support.
  AC_CACHE_CHECK([for ISA level support],
		 libc_cv_have_x86_isa_level, [dnl
cat > conftest.c <<EOF
#include <sysdeps/x86/isa-level.h>
#if MINIMUM_X86_ISA_LEVEL >= 4
libc_cv_have_x86_isa_level=4
#elif MINIMUM_X86_ISA_LEVEL == 3
libc_cv_have_x86_isa_level=3
#elif MINIMUM_X86_ISA_LEVEL == 2
libc_cv_have_x86_isa_level=2
#elif defined __x86_64__
libc_cv_have_x86_isa_level=baseline
#elif MINIMUM_X86_ISA_LEVEL == 1
libc_cv_have_x86_isa_level=1
#else
libc_cv_have_x86_isa_level=0
#endif
EOF
		 eval `${CC-cc} $CFLAGS $CPPFLAGS $ISAFLAG -I$srcdir -E conftest.c | grep libc_cv_have_x86_isa_level`
		 rm -rf conftest*])
elif test $base_machine = x86_64; then
  libc_cv_have_x86_isa_level=baseline
else
  libc_cv_have_x86_isa_level=0
fi
if test $libc_cv_have_x86_isa_level = baseline; then
  AC_DEFINE_UNQUOTED(MINIMUM_X86_ISA_LEVEL, 1)
else
  AC_DEFINE_UNQUOTED(MINIMUM_X86_ISA_LEVEL, $libc_cv_have_x86_isa_level)
fi
LIBC_CONFIG_VAR([have-x86-isa-level], [$libc_cv_have_x86_isa_level])
LIBC_CONFIG_VAR([x86-isa-level-2-or-above], [2 3 4])
LIBC_CONFIG_VAR([x86-isa-level-3-or-above], [3 4])
LIBC_CONFIG_VAR([x86-isa-level-4-or-above], [4])
LIBC_CONFIG_VAR([enable-x86-isa-level], [$libc_cv_include_x86_isa_level])

dnl Check if TEST_CC supports -mfpmath=387
LIBC_TRY_TEST_CC_OPTION([if -mfpmath=387 works],
  [-Werror -mfpmath=387],
  libc_cv_have_test_cc_cflags_mfpmath_387,
  [libc_cv_have_test_cc_cflags_mfpmath_387=yes],
  [libc_cv_have_test_cc_cflags_mfpmath_387=no]
)
LIBC_CONFIG_VAR(have-test-cc-cflags-mfpmath-387,
		$libc_cv_have_test_cc_cflags_mfpmath_387)

conftest_code="
extern int foo;
int *
foo_p (void)
{
  return &foo;
}
"
dnl Check if CC enables no direct extern access by default.
LIBC_TRY_CC_COMMAND([if no direct extern access is enabled by default],
  [$conftest_code],
  [-S -O2 -fno-pic],
  libc_cv_cc_no_direct_extern_access,
  [
libc_cv_cc_cflags_no_direct_extern_access=no
if test "$base_machine" = x86_64; then
  if grep -E -q "mov[lq][ 	]+foo@GOTPCREL\(%rip\)," conftest; then
    libc_cv_cc_no_direct_extern_access=yes
  fi
elif test "$base_machine" = i386; then
  if grep -E -q "movl[ 	]+foo@GOT," conftest; then
    libc_cv_cc_no_direct_extern_access=yes
  fi
else
  echo unsupported machine: $base_machine
  rm -f conftest*
  exit 1
fi
  ],
  [
echo failed to check no direct external data access
rm -f conftest*
exit 1
])

dnl Check if TEST_CC supports -mno-direct-extern-access.
LIBC_TRY_TEST_CC_OPTION([if -mno-direct-extern-access works],
  [-Werror -mno-direct-extern-access],
  libc_cv_test_cc_cflags_mno_direct_extern_access,
  [libc_cv_test_cc_cflags_mno_direct_extern_access=yes],
  [libc_cv_test_cc_cflags_mno_direct_extern_access=no]
)

dnl Check if TEST_CC supports -fno-direct-access-external-data.
LIBC_TRY_TEST_CC_OPTION([if -fno-direct-access-external-data works],
  [-Werror -fno-direct-access-external-data],
  libc_cv_test_cc_cflags_fno_direct_access_external_data,
  [libc_cv_test_cc_cflags_fno_direct_access_external_data=yes],
  [libc_cv_test_cc_cflags_fno_direct_access_external_data=no]
)

if test $libc_cv_test_cc_cflags_mno_direct_extern_access = yes; then
  libc_cv_test_cc_cflags_no_direct_extern_access="-mno-direct-extern-access"
elif test $libc_cv_test_cc_cflags_fno_direct_access_external_data = yes; then
  libc_cv_test_cc_cflags_no_direct_extern_access="-fno-direct-access-external-data"
fi

LIBC_CONFIG_VAR(test-cc-cflags-no-direct-extern-access,
		$libc_cv_test_cc_cflags_no_direct_extern_access)

conftest_code="
extern float truncf (float __x) __attribute__ ((__nothrow__,__const__));

float
tf (float x)
{
  return truncf (x);
}
"
dnl Check if CC inlines trunc with -Os.
LIBC_TRY_CC_COMMAND([if -Os inlines trunc],
  [$conftest_code],
  [-S -Os -msse4.1],
  libc_cv_cc_x86_inline_trunc,
  [
libc_cv_cc_x86_inline_trunc=no
if grep -E -q "roundss" conftest; then
  libc_cv_cc_x86_inline_trunc=yes
fi
],
[
echo "failed to check if -Os inlines trunc."
rm -f conftest*
exit 1
])
if test "$libc_cv_cc_x86_inline_trunc" = yes; then
  AC_DEFINE(HAVE_X86_INLINE_TRUNC, 1)
else
  AC_DEFINE(HAVE_X86_INLINE_TRUNC, 0)
fi


conftest_code="
int __gcc_CMPtype_GCC __attribute__ ((mode (__libgcc_cmp_return__)));
"
dnl Check if CC support the __libgcc_cmp_return__ mode attribute
LIBC_TRY_CC_COMMAND([for __libgcc_cmp_return__ mode attribute support],
  [$conftest_code],
  [$CFLAGS -c],
  libc_cv_cc_x86_libgcc_cmp_return_attribute,
  [libc_cv_cc_x86_libgcc_cmp_return_attribute=yes],
  [libc_cv_cc_x86_libgcc_cmp_return_attribute=no])
if test "$libc_cv_cc_x86_libgcc_cmp_return_attribute" = yes; then
  AC_DEFINE(HAVE_X86_LIBGCC_CMP_RETURN_ATTR, 1)
else
  AC_DEFINE(HAVE_X86_LIBGCC_CMP_RETURN_ATTR, 0)
fi

conftest_code="
double foo (double x, double y)
{
  return __builtin_fmod (x, y);
}
"
dnl Check if CC inlines __builtin_fmod/__builtin_remainder
LIBC_TRY_CC_COMMAND([if compiler inlines __builtin_fmod/__builtin_remainder],
  [$conftest_code],
  [$CFLAGS -fno-math-errno -S],
  libc_cv_cc_x86_inline_fmod,
  [
libc_cv_cc_x86_inline_fmod=no
if grep -E -q "fprem" conftest; then
  libc_cv_cc_x86_inline_fmod=yes
fi
],
[
echo "failed to check if CC inlines fmod."
rm -f conftest*
exit 1
])
if test "$libc_cv_cc_x86_inline_fmod" = yes; then
  AC_DEFINE(HAVE_X86_INLINE_FMOD, 1)
else
  AC_DEFINE(HAVE_X86_INLINE_FMOD, 0)
fi

dnl If the building compiler enables no direct external data access by
dnl default, access to protected data in shared libraries from executables
dnl must be compiled with no direct external data access.  If the testing
dnl compiler doesn't support it, set have-protected-data to no to disable
dnl the tests which requires no direct external data access.
dnl
if test "${libc_cv_cc_no_direct_extern_access}${libc_cv_test_cc_cflags_no_direct_extern_access}" = yes; then
  libc_cv_protected_data=no
fi

dnl Static PIE is supported.
AC_DEFINE(SUPPORT_STATIC_PIE)
