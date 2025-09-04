GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/loongarch/elf.

dnl It is always possible to access static and hidden symbols in an
dnl position independent way.
AC_DEFINE(HIDDEN_VAR_NEEDS_DYNAMIC_RELOC)

dnl Test if the toolchain is new enough for static PIE.
dnl We need a GAS supporting explicit reloc (older GAS produces stack-based
dnl reloc and triggers an internal error in the linker).  And, we need GCC to
dnl pass the correct linker flags for static PIE.  We strictly require GAS >=
dnl 2.41 so we don't need to check the assembler capability, but we need to
dnl check if GCC is doing the correct thing.
AC_CACHE_CHECK([if ${CC-cc} is sufficient to build static PIE on LoongArch],
libc_cv_static_pie_on_loongarch, [
  cat > conftest.S <<\EOF
.global _start
.type _start, @function
_start:
  li.w $a7, 93
  li.w $a0, 0
  syscall 0

.data
x:
  .word 0
  /* This should produce an R_LARCH_RELATIVE in the static PIE.  */
  .dword _start
EOF

  libc_cv_static_pie_on_loongarch=no
  if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -static-pie -nostdlib -fPIE -o conftest conftest.S]) \
     && AC_TRY_COMMAND([LC_ALL=C $READELF -Wr conftest | grep -q R_LARCH_RELATIVE]) \
     && ! AC_TRY_COMMAND([LC_ALL=C $READELF -Wl conftest | grep -q INTERP])
  then
    libc_cv_static_pie_on_loongarch=yes
  fi
  rm -rf conftest* ])

if test "$libc_cv_static_pie_on_loongarch" = yes; then
  AC_DEFINE(SUPPORT_STATIC_PIE)
fi

  # Check if gcc supports option -mcmodel=medium.
AC_CACHE_CHECK(whether $CC supports option -mcmodel=medium,
	       libc_cv_loongarch_cmodel_medium, [
  if AC_TRY_COMMAND(${CC-cc} -c $CFLAGS -mcmodel=medium /dev/null 1>&AS_MESSAGE_LOG_FD); then
    libc_cv_loongarch_cmodel_medium=yes
  else
    libc_cv_loongarch_cmodel_medium=no
  fi])
LIBC_CONFIG_VAR([have-cmodel-medium], [$libc_cv_loongarch_cmodel_medium])

# Check if asm support vector instructions.
AC_CACHE_CHECK(for vector support in assembler, libc_cv_loongarch_vec_asm, [dnl
cat > conftest.s <<\EOF
        vld  $vr0, $sp, 0
EOF
if AC_TRY_COMMAND(${CC-cc} -c $CFLAGS conftest.s -o conftest 1>&AS_MESSAGE_LOG_FD); then
  libc_cv_loongarch_vec_asm=yes
else
  libc_cv_loongarch_vec_asm=no
fi
rm -f conftest*])
if test $libc_cv_loongarch_vec_asm = no; then
  AC_MSG_ERROR([binutils version is too old, use 2.41 or newer version])
fi

AC_CACHE_CHECK([for vector support in compiler],
		libc_cv_loongarch_vec_com, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
void foo (void)
{
  asm volatile ("vldi \$vr0, 1" ::: "\$vr0");
  asm volatile ("xvldi \$xr0, 1" ::: "\$xr0");
}
]])],
                [libc_cv_loongarch_vec_com=yes],
                [libc_cv_loongarch_vec_com=no])])
if test "$libc_cv_loongarch_vec_com" = yes ;
then
  AC_DEFINE(HAVE_LOONGARCH_VEC_COM)
fi
LIBC_CONFIG_VAR([loongarch-vec-com], [$libc_cv_loongarch_vec_com])
