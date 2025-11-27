AC_PREREQ([2.63])
AC_INIT([gi-sample-autotools], [0.0.1])

AM_INIT_AUTOMAKE([1.11 no-define foreign])
AM_SILENT_RULES([yes])

LT_PREREQ([2.2])
LT_INIT([disable-static])

PKG_CHECK_MODULES(GISAMPLE, [gobject-2.0])
GOBJECT_INTROSPECTION_CHECK([1.30.0])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
