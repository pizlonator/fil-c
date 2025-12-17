#!/bin/sh

# Copyright (C) 2023 Elias Gustafsson
#
# This file is part of GnuTLS.
#
# GnuTLS is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at
# your option) any later version.
#
# GnuTLS is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

#set -e

srcdir=.
CERTTOOL=../../src/certtool${EXEEXT}
OUTFILE=out.$$.tmp
SERIAL_NUMBER=0xf12345
TMPFILE=tmp-negative-serial.pem.$$.tmp

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND}"
fi

${VALGRIND} "${CERTTOOL}" --generate-self-signed \
    --load-privkey "${srcdir}/../../doc/credentials/x509/key-rsa.pem" \
    --load-ca-privkey "${srcdir}/../../doc/credentials/x509/ca-key.pem" \
    --load-ca-certificate "${srcdir}/../../doc/credentials/x509/ca.pem" \
    --template "${srcdir}/templates/template-negative-serial.tmpl" \
    --outfile ${TMPFILE}
rc=$?

rm ${TMPFILE}

if test "${rc}" = "0";then
	echo "negative serial number was accepted"
	exit 1
fi
