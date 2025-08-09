AC_DEFUN([AX_PROGVAR], [
          test -n "$m4_toupper($1)" || { AC_PATH_PROG(m4_toupper($1), m4_default($2,$1)) }
          test -n "$m4_toupper($1)" || AC_MSG_ERROR([m4_default($2,$1) is required])
         ])

