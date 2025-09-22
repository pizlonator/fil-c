dnl Copyright (C) 2003, 2008  Silicon Graphics, Inc.
dnl
dnl This program is free software: you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.
AC_DEFUN([AC_FUNC_GCC_VISIBILITY],
  [AC_CACHE_CHECK(whether __attribute__((visibility())) is supported,
		  libc_cv_visibility_attribute,
		  [cat > conftest.c <<EOF
		   int foo __attribute__ ((visibility ("hidden"))) = 1;
		   int bar __attribute__ ((visibility ("protected"))) = 1;
EOF
		  libc_cv_visibility_attribute=no
		  if ${CC-cc} -Werror -S conftest.c -o conftest.s \
			>/dev/null 2>&1; then
		    if grep '\.hidden.*foo' conftest.s >/dev/null; then
		      if grep '\.protected.*bar' conftest.s >/dev/null; then
			libc_cv_visibility_attribute=yes
		      fi
		    fi
		  fi
		  rm -f conftest.[cs]
		  ])
   if test $libc_cv_visibility_attribute = yes; then
     AC_DEFINE(HAVE_VISIBILITY_ATTRIBUTE, [], [GCC supports visibility attributes])
     AC_DEFINE(EXPORT, [__attribute__ ((visibility ("default"))) extern], [Visibility attribute for API symbols])
     CFLAGS="${CFLAGS} -fvisibility=hidden"
   else
     AC_DEFINE(EXPORT, [extern], [Visibility attribute for API symbols])
   fi
  ])
