# pipeline-tar-sort-name.m4 serial 1
dnl PIPELINE_TAR_SORT_NAME
dnl Use the --sort=name option of GNU tar if it is available.
dnl Note that this only works with Automake's default tar-v7 option.

AC_DEFUN([PIPELINE_TAR_SORT_NAME],
[
AC_BEFORE([AM_INIT_AUTOMAKE], [$0])
AC_MSG_CHECKING([if tar --sort=name works])
rm -rf conftest.dir
mkdir conftest.dir
echo GrepMe > conftest.dir/file
AM_RUN_LOG([${TAR-tar} chof - conftest.dir --sort=name >conftest.tar])
if test -s conftest.tar; then
       AC_MSG_RESULT([yes])
       am__tar="$am__tar --sort=name"
else
       AC_MSG_RESULT([no])
fi
rm -rf conftest.dir
])
