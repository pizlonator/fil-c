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
  if test "$count" = 1; then
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
LIBC_CONFIG_VAR([x86-isa-level-3-or-above], [3 4])
LIBC_CONFIG_VAR([enable-x86-isa-level], [$libc_cv_include_x86_isa_level])

dnl Static PIE is supported.
AC_DEFINE(SUPPORT_STATIC_PIE)
