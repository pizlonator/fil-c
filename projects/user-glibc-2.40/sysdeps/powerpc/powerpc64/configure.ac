GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/powerpc/powerpc64.

AC_DEFINE(HIDDEN_VAR_NEEDS_DYNAMIC_RELOC)

AC_CACHE_CHECK(for support for overlapping .opd entries,
libc_cv_overlapping_opd, [dnl
libc_cv_overlapping_opd=no
echo 'void foo (void) {}' > conftest.c
if AC_TRY_COMMAND(${CC-cc} $CFLAGS $CPPFLAGS $libc_cv_cc_submachine -S conftest.c -o conftest.s 1>&AS_MESSAGE_LOG_FD); then
changequote(,)dnl
  if grep '\.TOC\.@tocbase' conftest.s > /dev/null; then
    if grep '\.TOC\.@tocbase[ 	]*,[ 	]*0' conftest.s > /dev/null; then
      :
    else
      libc_cv_overlapping_opd=yes
    fi
  fi
changequote([,])dnl
fi
rm -f conftest.c conftest.s
])
if test x$libc_cv_overlapping_opd = xyes; then
  AC_DEFINE(USE_PPC64_OVERLAPPING_OPD)
fi

# We check if compiler supports @notoc generation since there is no
# gain by enabling it if it will be optimized away by the linker.
# It also helps linkers that might not optimize it and end up
# generating stubs with ISA 3.1 instruction even targeting older ISA.
AC_CACHE_CHECK([if the compiler supports @notoc],
	       libc_cv_ppc64_notoc, [dnl
  cat > conftest.c <<EOF
int bar (void);
int foo (void) { return bar () + 1; }
EOF
  libc_cv_ppc64_notoc=no
  if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $libc_cv_cc_submachine -S -o conftest.s conftest.c]) \
     && AC_TRY_COMMAND([grep -q -E 'bar@notoc' conftest.s])
  then
    libc_cv_ppc64_notoc=yes
  fi
  rm -rf conftest.*])
AS_IF([test x$libc_cv_ppc64_notoc = xyes],
      [AC_DEFINE(USE_PPC64_NOTOC)])
