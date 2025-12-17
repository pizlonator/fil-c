#!/bin/sh

# Copyright (C) 2024 Daiki Ueno
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
# You should have received a copy of the GNU General Public License
# along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.

#set -e

: ${srcdir=.}
: ${CERTTOOL=../../src/certtool${EXEEXT}}

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND}"
fi

. ${srcdir}/../scripts/common.sh

testdir=`create_testdir rsa-oaep`

# Create an RSA-OAEP private key, restricted to the use with RSA-OAEP
${VALGRIND} "${CERTTOOL}" --generate-privkey \
            --key-type rsa-oaep --outfile "$testdir/key-rsa-oaep.pem"
rc=$?

if test "${rc}" != "0"; then
    echo "Could not generate an RSA-OAEP key"
    exit 1
fi

for i in sha256 sha384 sha512; do
    if test "${GNUTLS_FORCE_FIPS_MODE}" = 1 && test "$i" != sha384;then
	continue
    fi

    # Create an RSA-OAEP private key, restricted to the use with RSA-OAEP
    ${VALGRIND} "${CERTTOOL}" --generate-privkey --pkcs8 --empty-password \
		--key-type rsa-oaep --hash $i --outfile \
		"$testdir/key-rsa-oaep-$i.pem"
    rc=$?

    if test "${rc}" != "0"; then
	echo "Could not generate an RSA-OAEP key ($i)"
	exit 1
    fi

    ${VALGRIND} "${CERTTOOL}" -k --empty-password --infile "$testdir/key-rsa-oaep-$i.pem" >/dev/null
    rc=$?
    if test "${rc}" != "0"; then
	echo "Could not read generated an RSA-OAEP key ($i)"
	exit 1
    fi

    # Create an RSA-OAEP certificate from an RSA key
    ${VALGRIND} "${CERTTOOL}" --generate-certificate --key-type rsa-oaep \
		--load-ca-privkey "${srcdir}/../../doc/credentials/x509/ca-key.pem" \
		--load-ca-certificate "${srcdir}/../../doc/credentials/x509/ca.pem" \
		--load-privkey "$testdir/key-rsa-oaep-$i.pem" \
		--template "${srcdir}/templates/template-encryption-only.tmpl" \
		--outfile "$testdir/cert-rsa-oaep-$i.pem" --hash $i
    rc=$?

    if test "${rc}" != "0"; then
	echo "Could not generate an RSA-OAEP certificate $i"
	exit 1
    fi

    ${CERTTOOL} -i --infile "$testdir/cert-rsa-oaep-$i.pem" | grep -i "Subject Public Key Algorithm: RSA-OAEP"
    if test $? != 0;then
	echo "Generated certificate is not RSA-OAEP"
	cat ${TMPFILE}
	exit 1
    fi
done

rm -rf "${testdir}"
