#!/bin/sh

# Copyright (C) 2021-2022 Free Software Foundation, Inc.
# Copyright (C) 2019 Red Hat, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

PARSER="${PARSER:-../src/asn1Parser${EXEEXT}}"
srcdir="${srcdir:-.}"
TMPFILE=pkix.asn.$$.tmp

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute valgrind --leak-check=full"
fi

echo "Test: PKIX file generation"

${VALGRIND} ${PARSER} ${srcdir}/pkix.asn -o ${TMPFILE}

if test $? != 0;then
	echo "Cannot generate C file!"
	exit 1
fi

# Find out how to remove carriage returns from output. Solaris /usr/ucb/tr
# does not understand '\r'.
if echo solaris | tr -d '\r' | grep solais > /dev/null; then
  cr='\015'
else
  cr='\r'
fi
# normalize output
LC_ALL=C tr -d "$cr" < $TMPFILE > x$TMPFILE
mv x$TMPFILE $TMPFILE

diff ${TMPFILE} ${srcdir}/pkix.asn.out

if test $? != 0;then
	echo "Generated C file differs!"
	exit 1
fi

rm -f ${TMPFILE}

exit 0
