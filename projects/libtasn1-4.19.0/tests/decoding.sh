#!/bin/sh

# Copyright (C) 2015-2022 Free Software Foundation, Inc.
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

ASN1DECODING="${ASN1DECODING:-../src/asn1Decoding$EXEEXT}"
ASN1PKIX="${ASN1PKIX:-pkix.asn}"

$VALGRIND $ASN1DECODING $ASN1PKIX ${srcdir}/TestCertOctetOverflow.der PKIX1.Certificate
if test $? != 1;then
	echo "Decoding failed"
	exit 1
fi

# test decoding of certificate with invalid time field
$VALGRIND $ASN1DECODING -s $ASN1PKIX ${srcdir}/cert-invalid-time.der PKIX1.Certificate
if test $? != 1;then
	echo "Decoding with invalid time succeeded when not expected"
	exit 1
fi

# test decoding of certificate with invalid time field
$VALGRIND $ASN1DECODING -t $ASN1PKIX ${srcdir}/cert-invalid-time.der PKIX1.Certificate
if test $? != 0;then
	echo "Decoding with invalid time failed when not expected"
	exit 1
fi

exit 0
