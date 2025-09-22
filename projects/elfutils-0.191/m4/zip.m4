dnl -*- Autoconf -*- test for either zlib or bzlib.
dnl Defines --with-$1 argument, $2 automake conditional,
dnl and sets AC_DEFINE(USE_$2) and LIBS.

AC_DEFUN([eu_ZIPLIB], [dnl
AC_ARG_WITH([[$1]],
AS_HELP_STRING([--with-[$1]], [support [$1] compression in libdwfl]),,
	    [with_[$1]=default])
if test $with_[$1] != no; then
  AC_SEARCH_LIBS([$4], [$3], [with_[$1]=yes],
  	         [test $with_[$1] = default ||
		  AC_MSG_ERROR([missing -l[$3] for --with-[$1]])])
fi
AM_CONDITIONAL([$2], test $with_[$1] = yes)
if test $with_[$1] = yes; then
  AC_DEFINE(USE_[$2])
else
  with_[$1]=no
fi
AH_TEMPLATE(USE_[$2], [Support $5 decompression via -l$3.])])
