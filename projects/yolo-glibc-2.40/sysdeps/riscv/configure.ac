GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/riscv/elf.

# Check if static linker supports R_RISCV_ALIGN
AC_CACHE_CHECK([for R_RISCV_ALIGN linker relaxation support], [libc_cv_riscv_r_align],[dnl
  cat > conftest.S <<EOF
.align 3
foo:
  li a0,42
  ret
EOF
  libc_cv_riscv_r_align=no
  if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -nostdlib -nostartfiles $no_ssp -shared -fPIC -o contests.o conftest.S])
  then
    libc_cv_riscv_r_align=yes
  fi
  rm -rf conftest.*])
LIBC_CONFIG_VAR([riscv-r-align], [$libc_cv_riscv_r_align])

dnl Test if the toolchain is new enough for static PIE.
AC_CACHE_CHECK([if the toolchain is sufficient to build static PIE on RISC-V],
libc_cv_static_pie_on_riscv, [
  cat > conftest1.S <<\EOF
    .globl _start
    .type   _start, @function
_start:
    nop

    .data
    /* This should produce an R_RISCV_RELATIVE in the static PIE.  */
    .dword      _start
EOF

  libc_cv_static_pie_on_riscv=no
  if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -static-pie -nostdlib -fPIE -o conftest1 conftest1.S]) \
     && AC_TRY_COMMAND([LC_ALL=C $READELF -Wr conftest1 | grep -q R_RISCV_RELATIVE]) \
     && ! AC_TRY_COMMAND([LC_ALL=C $READELF -Wl conftest1 | grep -q INTERP])
  then
    libc_cv_static_pie_on_riscv=yes
  fi
  rm -rf conftest* ])

if test "$libc_cv_static_pie_on_riscv" = yes; then
  AC_DEFINE(SUPPORT_STATIC_PIE)
fi
