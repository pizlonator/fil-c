GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/s390.

AC_CACHE_CHECK([for S390 vector instruction support], libc_cv_asm_s390_vx, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void testvecinsn ()
{
    __asm__ (".machine \"z13\" \n\t"
	     "vistrbs %%v16,%%v17 \n\t"
	     "locghie %%r1,0" : :);
}
]])],
		  [libc_cv_asm_s390_vx=yes],
		  [libc_cv_asm_s390_vx=no])])
if test "$libc_cv_asm_s390_vx" = yes ;
then
  AC_DEFINE(HAVE_S390_VX_ASM_SUPPORT)
else
  AC_MSG_WARN([Use binutils with vector-support in order to use optimized implementations.])
fi


dnl test, if gcc supports S390 vector registers as clobber in inline assembly
AC_CACHE_CHECK([for S390 vector support in gcc], libc_cv_gcc_s390_vx, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void testvecclobber ()
{
  __asm__ ("" : : : "v16");
}
]])],
		  [libc_cv_gcc_s390_vx=yes],
		  [libc_cv_gcc_s390_vx=no])])
if test "$libc_cv_gcc_s390_vx" = yes ;
then
  AC_DEFINE(HAVE_S390_VX_GCC_SUPPORT)
fi


AC_CACHE_CHECK([for S390 arch13 zarch instruction support],
	       libc_cv_asm_s390_arch13, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void testinsn (char *buf)
{
    __asm__ (".machine \"arch13\" \n\t"
	     "lghi %%r0,16 \n\t"
	     "mvcrl 0(%0),32(%0) \n\t"
	     "vstrs %%v20,%%v20,%%v20,%%v20,0,2"
	     : : "a" (buf) : "memory", "r0");
}
]])],
		  [libc_cv_asm_s390_arch13=yes],
		  [libc_cv_asm_s390_arch13=no])])
if test "$libc_cv_asm_s390_arch13" = yes ;
then
  AC_DEFINE(HAVE_S390_ARCH13_ASM_SUPPORT)
fi


AC_CACHE_CHECK([for S390 z10 zarch instruction support as default],
	       libc_cv_asm_s390_min_z10_zarch, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void testinsn (void *a, void *b, int n)
{
    __asm__ ("exrl %2,1f \n\t"
	     "j 2f \n\t"
	     "1: mvc 0(1,%0),0(%1) \n\t"
	     "2:"
	     : : "a" (a), "a" (b), "d" (n)
	     : "memory", "cc");
}
]])],
		  [libc_cv_asm_s390_min_z10_zarch=yes],
		  [libc_cv_asm_s390_min_z10_zarch=no])])
if test "$libc_cv_asm_s390_min_z10_zarch" = yes ;
then
  AC_DEFINE(HAVE_S390_MIN_Z10_ZARCH_ASM_SUPPORT)
fi


AC_CACHE_CHECK([for S390 z196 zarch instruction support as default],
	       libc_cv_asm_s390_min_z196_zarch, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
float testinsn (double e)
{
    float d;
    __asm__ ("ledbra %0,5,%1,4" : "=f" (d) : "f" (e) );
    return d;
}
]])],
		  [libc_cv_asm_s390_min_z196_zarch=yes],
		  [libc_cv_asm_s390_min_z196_zarch=no])])
if test "$libc_cv_asm_s390_min_z196_zarch" = yes ;
then
  AC_DEFINE(HAVE_S390_MIN_Z196_ZARCH_ASM_SUPPORT)
fi


AC_CACHE_CHECK([for S390 z13 zarch instruction support as default],
	       libc_cv_asm_s390_min_z13_zarch, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
int testinsn (void)
{
    int i;
    __asm__ ("vl %%v16,0(%%r15)\n\t"
	     "vlgvf %0,%%v16,0"
	     : "=d" (i) : : "memory", "v16");
    return i;
}
]])],
		  [libc_cv_asm_s390_min_z13_zarch=yes],
		  [libc_cv_asm_s390_min_z13_zarch=no])])
if test "$libc_cv_asm_s390_min_z13_zarch" = yes ;
then
  AC_DEFINE(HAVE_S390_MIN_Z13_ZARCH_ASM_SUPPORT)
fi


AC_CACHE_CHECK([for S390 arch13 zarch instruction support as default],
	       libc_cv_asm_s390_min_arch13_zarch, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void testinsn (char *buf)
{
    __asm__ ("lghi %%r0,16 \n\t"
	     "mvcrl 0(%0),32(%0) \n\t"
	     "vstrs %%v20,%%v20,%%v20,%%v20,0,2"
	     : : "a" (buf) : "memory", "r0");
}
]])],
		  [libc_cv_asm_s390_min_arch13_zarch=yes],
		  [libc_cv_asm_s390_min_arch13_zarch=no])])
if test "$libc_cv_asm_s390_min_arch13_zarch" = yes ;
then
  AC_DEFINE(HAVE_S390_MIN_ARCH13_ZARCH_ASM_SUPPORT)
fi

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
  # (by using -static-pie).
  # If the R_390_IRELATIVE relocation lands in .rela.plt, ensure that there is
  # DT_JMPREL pointing to it (binutils).  Otherwise there should be a .rela.dyn
  # without DT_JMPREL (lld>=19).
  if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -fPIE -c conftest1.c -o conftest1.o]) \
     && AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -fPIE -c conftest2.c -o conftest2.o]) \
     && AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -static-pie -nostartfiles -nostdlib -fPIE -o conftest conftest1.o conftest2.o]) \
     && AC_TRY_COMMAND([! LC_ALL=C $READELF -Wr conftest | grep R_390_TLS_TPOFF] >&AS_MESSAGE_LOG_FD) \
     && AC_TRY_COMMAND([LC_ALL=C $READELF -Wr conftest | grep R_390_IRELATIVE] >&AS_MESSAGE_LOG_FD) \
     && ( AC_TRY_COMMAND([LC_ALL=C $READELF -Wr conftest | grep \.rela\.plt >&AS_MESSAGE_LOG_FD]) \
	  && AC_TRY_COMMAND([LC_ALL=C $READELF -Wd conftest | grep JMPREL >&AS_MESSAGE_LOG_FD]) \
	  || AC_TRY_COMMAND([LC_ALL=C $READELF -Wr conftest | grep \.rela\.dyn >&AS_MESSAGE_LOG_FD]) )
  then
    libc_cv_s390x_staticpie_req_runtime=yes
  fi
  rm -rf conftest*])

if test $libc_cv_s390x_staticpie_req_runtime = yes; then
   # Some kernels might fail with /proc/sys/kernel/randomize_va_space set to 0
   # due not enough space for a brk call.  However, there is no reliable way to
   # test it.
   AC_DEFINE(SUPPORT_STATIC_PIE)
fi
