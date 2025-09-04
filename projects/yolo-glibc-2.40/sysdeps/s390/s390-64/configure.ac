GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/s390/s390-64.

# Minimal checking for static PIE support in ld.
# Compare to ld testcase/bugzilla:
# <binutils-source>/ld/testsuite/ld-elf/pr22263-1.rd
AC_CACHE_CHECK([for s390-specific static PIE requirements], \
[libc_cv_s390x_staticpie_req_runtime], [dnl
  cat > conftest1.c <<EOF
__thread int * foo;

void
bar (void)
{
  *foo = 1;
}

void bar2 (void);
void* bar2_ifunc (void) __asm__ ("bar2");
__asm__ (".type bar2, %gnu_indirect_function");
void* bar2_ifunc (void) { return 0; }
EOF
  cat > conftest2.c <<EOF
extern __thread int *foo;
extern void bar (void);
extern void bar2 (void);
static int x;

int
main ()
{
  foo = &x;
  bar2 ();
  return 0;
}
EOF
  libc_cv_s390x_staticpie_req_runtime=no
  # Check if the static linker does not generate dynamic TLS relocs in PIE
  # (binutils PR ld/22263), if it accepts --no-dynamic-linker
  # (by using -static-pie), and if it adds a DT_JMPREL pointing to .rela.iplt
  # with static pie.
  if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -fPIE -c conftest1.c -o conftest1.o]) \
     && AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -fPIE -c conftest2.c -o conftest2.o]) \
     && AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -static-pie -nostartfiles -nostdlib -fPIE -o conftest conftest1.o conftest2.o]) \
     && AC_TRY_COMMAND([! LC_ALL=C $READELF -Wr conftest | grep R_390_TLS_TPOFF] >&AS_MESSAGE_LOG_FD) \
     && AC_TRY_COMMAND([LC_ALL=C $READELF -Wd conftest | grep JMPREL >&AS_MESSAGE_LOG_FD])
  then
    libc_cv_s390x_staticpie_req_runtime=yes
  fi
  rm -rf conftest.*])

if test $libc_cv_s390x_staticpie_req_runtime = yes; then
   # Some kernels might fail with /proc/sys/kernel/randomize_va_space set to 0
   # due not enough space for a brk call.  However, there is no reliable way to
   # test it.
   AC_DEFINE(SUPPORT_STATIC_PIE)
fi
