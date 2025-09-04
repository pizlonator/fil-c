GLIBC_PROVIDES dnl See aclocal.m4 in the top level source directory.
# Local configure fragment for sysdeps/microblaze.

AC_DEFINE(HIDDEN_VAR_NEEDS_DYNAMIC_RELOC)

# gcc 11.2.1 and earlier crash with an internal compiler error, see:
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103613
AC_DEFINE(PIE_UNSUPPORTED)
# work around problem with autoconf and empty lines at the end of files
