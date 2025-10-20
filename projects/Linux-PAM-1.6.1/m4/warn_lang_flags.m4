#!/usr/bin/m4

AC_DEFUN([pam_WARN_LANG_FLAGS], [dnl
gl_WARN_ADD([-W])
gl_WARN_ADD([-Wall])
gl_WARN_ADD([-Wbad-function-cast])
gl_WARN_ADD([-Wcast-align])
gl_WARN_ADD([-Wcast-align=strict])
gl_WARN_ADD([-Wcast-qual])
gl_WARN_ADD([-Wdeprecated])
gl_WARN_ADD([-Wformat=2])
gl_WARN_ADD([-Winit-self])
gl_WARN_ADD([-Winline])
gl_WARN_ADD([-Wmain])
gl_WARN_ADD([-Wmissing-declarations])
gl_WARN_ADD([-Wmissing-format-attribute])
gl_WARN_ADD([-Wmissing-prototypes])
gl_WARN_ADD([-Wnull-dereference])
gl_WARN_ADD([-Wp64])
gl_WARN_ADD([-Wpointer-arith])
gl_WARN_ADD([-Wreturn-type])
gl_WARN_ADD([-Wshadow])
gl_WARN_ADD([-Wstrict-prototypes])
gl_WARN_ADD([-Wundef])
gl_WARN_ADD([-Wuninitialized])
gl_WARN_ADD([-Wunused])
gl_WARN_ADD([-Wwrite-strings])
AC_ARG_ENABLE([Werror],
  [AS_HELP_STRING([--enable-Werror], [turn on -Werror compiler option])],
  [case $enableval in
     yes) gl_WARN_ADD([-Werror]) ;;
     no)  ;;
     *)   AC_MSG_ERROR([bad value $enableval for Werror option]) ;;
   esac])
AS_VAR_PUSHDEF([pam_WARN_FLAGS], [WARN_[]_AC_LANG_PREFIX[]FLAGS])dnl
AC_SUBST(pam_WARN_FLAGS)
AS_VAR_POPDEF([pam_WARN_FLAGS])dnl
])
