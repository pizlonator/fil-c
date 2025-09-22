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

if ! test -z "${VALGRIND}";then
VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=7"
fi
TMPFILE=decoding-invalid.$$.tmp
ASN1DECODING="${ASN1DECODING:-../src/asn1Decoding$EXEEXT}"
ASN1PKIX="${ASN1PKIX:-pkix.asn}"

# This tests an invalid input which caused an infinite recursion
# to certain libtasn1 versions.

for i in "${srcdir}/invalid-pkcs7/"*.der;do
$VALGRIND $ASN1DECODING "$ASN1PKIX" "$i" PKIX1.pkcs-7-ContentInfo >$TMPFILE 2>&1
ret=$?
if test $ret != 1;then
	echo "Decoding failed for $i"
	cat $TMPFILE
	exit 1
fi
echo "$(basename $i): ok"
done

rm -f $TMPFILE

exit 0
