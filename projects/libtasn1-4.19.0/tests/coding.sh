#!/bin/sh

# Copyright (C) 2017-2022 Free Software Foundation, Inc.
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

srcdir="${srcdir:-.}"

ASAN_OPTIONS="detect_leaks=0:exitcode=6"
export ASAN_OPTIONS

if ! test -z "${VALGRIND}";then
VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=7"
fi

ASN1CODING="${ASN1CODING:-../src/asn1Coding$EXEEXT}"
ASN1PKIX="${ASN1PKIX:-pkix.asn}"
TMPFILE="asn1.$$.tmp"

cat <<EOF >$TMPFILE
PKIX1 { }

DEFINITIONS IMPLICIT TAGS ::=

BEGIN

Dss-Sig-Value ::= SEQUENCE {
     r       INTEGER,
     s       INTEGER
}

END
EOF

$VALGRIND $ASN1CODING -c $TMPFILE ${srcdir}/invalid-assignments1.txt
if test $? != 1;then
	echo "Encoding failed (1)"
	exit 1
fi

$VALGRIND $ASN1CODING -c $TMPFILE ${srcdir}/invalid-assignments2.txt
if test $? != 1;then
	echo "Encoding failed (2)"
	exit 1
fi

rm -f $TMPFILE

exit 0
