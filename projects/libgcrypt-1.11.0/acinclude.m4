dnl macros to configure Libgcrypt
dnl Copyright (C) 1998, 1999, 2000, 2001, 2002,
dnl               2003 Free Software Foundation, Inc.
dnl Copyright (C) 2013 g10 Code GmbH
dnl
dnl This file is part of Libgcrypt.
dnl
dnl Libgcrypt is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU Lesser General Public License as
dnl published by the Free Software Foundation; either version 2.1 of
dnl the License, or (at your option) any later version.
dnl
dnl Libgcrypt is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU Lesser General Public License for more details.
dnl
dnl You should have received a copy of the GNU Lesser General Public
dnl License along with this program; if not, see <https://www.gnu.org/licenses/>.
dnl SPDX-License-Identifier: LGPL-2.1-or-later

dnl GCRY_MSG_SHOW(PREFIX,STRING)
dnl Print a message with a prefix.
dnl
define([GCRY_MSG_SHOW],
  [
     echo "        $1 $2" 1>&AS_MESSAGE_FD([])
  ])

dnl GCRY_MSG_WRAP(PREFIX, ALGOLIST)
dnl Print a nicely formatted list of algorithms
dnl with an appropriate line wrap.
dnl
define([GCRY_MSG_WRAP],
  [
    tmp="        $1"
    tmpi="abc"
    if test "${#tmpi}" -ne 3 >/dev/null 2>&1 ; then
      dnl Without a POSIX shell, we don't botter to wrap it
      echo "$tmp $2" 1>&AS_MESSAGE_FD([])
    else
      tmpi=`echo "$tmp"| sed 's/./ /g'`
      echo $2 EOF | tr ' ' '\n' | \
        while read word; do
          if test "${#tmp}" -gt 70 ; then
            echo "$tmp" 1>&AS_MESSAGE_FD([])
            tmp="$tmpi"
          fi
          if test "$word" = "EOF" ; then
            echo "$tmp" 1>&AS_MESSAGE_FD([])
          else
            tmp="$tmp $word"
          fi
        done
    fi
  ])


#
# GNUPG_SYS_SYMBOL_UNDERSCORE
# Does the compiler prefix global symbols with an underscore?
#
# Taken from GnuPG 1.2 and modified to use the libtool macros.
AC_DEFUN([GNUPG_SYS_SYMBOL_UNDERSCORE],
[tmp_do_check="no"
case "${host}" in
    i?86-mingw32* | i?86-*-mingw32*)
        ac_cv_sys_symbol_underscore=yes
        ;;
    x86_64-*-mingw32*)
        ac_cv_sys_symbol_underscore=no
        ;;
    i386-emx-os2 | i[3456]86-pc-os2*emx | i386-pc-msdosdjgpp)
        ac_cv_sys_symbol_underscore=yes
        ;;
    *)
      if test "$cross_compiling" != yes; then
         tmp_do_check="yes"
      fi
      ;;
esac
if test "$tmp_do_check" = "yes"; then
  AC_REQUIRE([AC_LIBTOOL_SYS_GLOBAL_SYMBOL_PIPE])
  AC_MSG_CHECKING([for _ prefix in compiled symbols])
  AC_CACHE_VAL(ac_cv_sys_symbol_underscore,
  [ac_cv_sys_symbol_underscore=no
   cat > conftest.$ac_ext <<EOF
      void nm_test_func(void){}
      int main(void){nm_test_func();return 0;}
EOF
  if AC_TRY_EVAL(ac_compile); then
    # Now try to grab the symbols.
    nlist=conftest.nm
    if AC_TRY_EVAL(NM conftest.$ac_objext \| "$lt_cv_sys_global_symbol_pipe" \> $nlist) && test -s "$nlist"; then
      # See whether the symbols have a leading underscore.
      if $GREP ' _nm_test_func$' "$nlist" >/dev/null; then
        ac_cv_sys_symbol_underscore=yes
      else
        if $GREP ' nm_test_func$' "$nlist" >/dev/null; then
          :
        else
          echo "configure: cannot find nm_test_func in $nlist" >&AS_MESSAGE_LOG_FD
        fi
      fi
    else
      echo "configure: cannot run $lt_cv_sys_global_symbol_pipe" >&AS_MESSAGE_LOG_FD
    fi
  else
    echo "configure: failed program was:" >&AS_MESSAGE_LOG_FD
    cat conftest.c >&AS_MESSAGE_LOG_FD
  fi
  rm -rf conftest*
  ])
  else
  AC_MSG_CHECKING([for _ prefix in compiled symbols])
  fi
AC_MSG_RESULT($ac_cv_sys_symbol_underscore)
if test x$ac_cv_sys_symbol_underscore = xyes; then
  AC_DEFINE(WITH_SYMBOL_UNDERSCORE,1,
            [Defined if compiled symbols have a leading underscore])
fi
])


######################################################################
# Check whether mlock is broken (hpux 10.20 raises a SIGBUS if mlock
# is not called from uid 0 (not tested whether uid 0 works)
# For DECs Tru64 we have also to check whether mlock is in librt
# mlock is there a macro using memlk()
######################################################################
dnl GNUPG_CHECK_MLOCK
dnl
define(GNUPG_CHECK_MLOCK,
  [ AC_CHECK_FUNCS(mlock)
    if test "$ac_cv_func_mlock" = "no"; then
        AC_CHECK_HEADERS(sys/mman.h)
        if test "$ac_cv_header_sys_mman_h" = "yes"; then
            # Add librt to LIBS:
            AC_CHECK_LIB(rt, memlk)
            AC_CACHE_CHECK([whether mlock is in sys/mman.h],
                            gnupg_cv_mlock_is_in_sys_mman,
                [AC_LINK_IFELSE(
                   [AC_LANG_PROGRAM([[
                    #include <assert.h>
                    #ifdef HAVE_SYS_MMAN_H
                    #include <sys/mman.h>
                    #endif
                    ]], [[
int i;

/* glibc defines this for functions which it implements
 * to always fail with ENOSYS.  Some functions are actually
 * named something starting with __ and the normal name
 * is an alias.  */
#if defined (__stub_mlock) || defined (__stub___mlock)
choke me
#else
mlock(&i, 4);
#endif
; return 0;
                    ]])],
                gnupg_cv_mlock_is_in_sys_mman=yes,
                gnupg_cv_mlock_is_in_sys_mman=no)])
            if test "$gnupg_cv_mlock_is_in_sys_mman" = "yes"; then
                AC_DEFINE(HAVE_MLOCK,1,
                          [Defined if the system supports an mlock() call])
            fi
        fi
    fi
    if test "$ac_cv_func_mlock" = "yes"; then
        AC_CHECK_FUNCS(sysconf getpagesize)
        AC_MSG_CHECKING(whether mlock is broken)
          AC_CACHE_VAL(gnupg_cv_have_broken_mlock,
             AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>

int main(void)
{
    char *pool;
    int err;
    long int pgsize;

#if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
    pgsize = sysconf (_SC_PAGESIZE);
#elif defined (HAVE_GETPAGESIZE)
    pgsize = getpagesize();
#else
    pgsize = -1;
#endif

    if (pgsize == -1)
      pgsize = 4096;

    pool = malloc( 4096 + pgsize );
    if( !pool )
        return 2;
    pool += (pgsize - ((size_t)pool % pgsize));

    err = mlock( pool, 4096 );
    if( !err || errno == EPERM || errno == EAGAIN)
        return 0; /* okay */

    return 1;  /* hmmm */
}
            ]])],
            gnupg_cv_have_broken_mlock="no",
            gnupg_cv_have_broken_mlock="yes",
            gnupg_cv_have_broken_mlock="assume-no"
           )
         )
         if test "$gnupg_cv_have_broken_mlock" = "yes"; then
             AC_DEFINE(HAVE_BROKEN_MLOCK,1,
                       [Defined if the mlock() call does not work])
             AC_MSG_RESULT(yes)
         else
            if test "$gnupg_cv_have_broken_mlock" = "no"; then
                AC_MSG_RESULT(no)
            else
                AC_MSG_RESULT(assuming no)
            fi
         fi
    fi
  ])

dnl LIST_MEMBER()
dnl Check whether an element ist contained in a list.  Set `found' to
dnl `1' if the element is found in the list, to `0' otherwise.
AC_DEFUN([LIST_MEMBER],
[
name=$1
list=$2
found=0

for n in $list; do
  if test "x$name" = "x$n"; then
    found=1
  fi
done
])
