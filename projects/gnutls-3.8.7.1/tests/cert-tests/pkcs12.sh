#!/bin/sh

# Copyright (C) 2004-2006, 2008, 2010, 2012 Free Software Foundation,
# Inc.
#
# Author: Simon Josefsson
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

: ${srcdir=.}
: ${CERTTOOL=../../src/certtool${EXEEXT}}

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

if test "${GNUTLS_FORCE_FIPS_MODE}" = 1;then
	echo "Cannot run in FIPS140-2 mode"
	exit 77
fi

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=1"
fi

: ${DIFF=diff}
DEBUG=""

. "${srcdir}/../scripts/common.sh"
testdir=`create_testdir pkcs12`

TMPFILE=$testdir/pkcs12
TMPFILE_PEM=$testdir/pkcs12.pem

DEBUG="1"

for p12 in "aes-128.p12 Red%20Hat%20Enterprise%20Linux%207.4" "pbes1-no-salt.p12 Red%20Hat%20Enterprise%20Linux%207.4" "no-salt.p12 Red%20Hat%20Enterprise%20Linux%207.4" "mac-sha512.p12 Red%20Hat%20Enterprise%20Linux%207.4" "cert-with-crl.p12 password" "client.p12 foobar" "openssl.p12 CaudFocwijRupogDoicsApfiHadManUgNa" "noclient.p12" "unclient.p12" "pkcs12_2certs.p12"; do
	set -- ${p12}
	file="$1"
	passwd=$(echo $2|sed 's/%20/ /g')

	if test "x$DEBUG" != "x"; then
		${VALGRIND} "${CERTTOOL}" -d 99 --p12-info --inder --password "${passwd}" \
			--infile "${srcdir}/data/${file}"
	else
		${VALGRIND} "${CERTTOOL}" --p12-info --inder --password "${passwd}" \
			--infile "${srcdir}/data/${file}" >/dev/null
	fi
	rc=$?
	if test ${rc} != 0; then
		echo "PKCS12 FATAL ${p12}"
		exit 1
	fi
done

file="$srcdir/data/test-null.p12"
${VALGRIND} "${CERTTOOL}" --p12-info --inder --null-password --infile "${file}" >/dev/null
rc=$?
if test ${rc} != 0; then
	echo "PKCS12 FATAL ${file}"
	exit 1
fi

file="$srcdir/data/sha256.p12"
${VALGRIND} "${CERTTOOL}" --p12-info --inder --password 1234 --infile "${file}" >/dev/null
rc=$?
if test ${rc} != 0; then
	echo "PKCS12 FATAL ${file}"
	exit 1
fi

${VALGRIND} "${CERTTOOL}" --p12-info --inder --password 1234 --infile "$srcdir/data/sha256.p12" --outfile "${TMPFILE}" --no-text
rc=$?

if test "${rc}" != "0"; then
	echo "--no-text pkcs12 info failed 1"
	exit ${rc}
fi

if grep -v '^-----BEGIN [A-Z0-9 ]\+-----$' ${TMPFILE} | grep -v '^[A-Za-z0-9/+=]\+$' | grep -v '^-----END [A-Z0-9 ]\+-----$' ; then
	echo "--no-text pkcs12 info failed 2"
	exit 1
fi

# test whether we can encode a certificate and a key
${VALGRIND} "${CERTTOOL}" --to-p12 --password 1234 --p12-name "my-key" --load-certificate "${srcdir}/../certs/cert-ecc256.pem" --load-privkey "${srcdir}/../certs/ecc256.pem" --outder --outfile $TMPFILE >/dev/null
rc=$?
if test ${rc} != 0; then
	echo "PKCS12 FATAL encoding"
	exit 1
fi

${VALGRIND} "${CERTTOOL}" --p12-info --inder --password 1234 --infile $TMPFILE|tr -d '\r' >${TMPFILE_PEM} 2>/dev/null
rc=$?
if test ${rc} != 0; then
	echo "PKCS12 FATAL decrypting/decoding"
	exit 1
fi

grep "BEGIN ENCRYPTED PRIVATE KEY" ${TMPFILE_PEM} >/dev/null 2>&1
rc=$?

if test "${rc}" != "0"; then
	exit ${rc}
fi

grep "BEGIN CERTIFICATE" ${TMPFILE_PEM} >/dev/null 2>&1
rc=$?

if test "${rc}" != "0"; then
	exit ${rc}
fi

INFO_EXP=$testdir/p12-info.exp
INFO_OUT=$testdir/p12-info.out

cat >$INFO_EXP <<EOF
MAC info:
	MAC: SHA256 (2.16.840.1.101.3.4.2.1)
	Salt size: 8
	Iteration count: $PKCS12_ITER_COUNT

BAG #0
	Type: Encrypted
	Cipher: AES-128-CBC
	Schema: PBES2-AES128-CBC (2.16.840.1.101.3.4.1.2)
	Iteration count: $PKCS12_ITER_COUNT

BAG #1
	Elements: 1
	Type: PKCS #8 Encrypted key
	PKCS #8 information:
		Cipher: AES-128-CBC
		Schema: PBES2-AES128-CBC (2.16.840.1.101.3.4.1.2)
		Iteration count: $PKCS12_ITER_COUNT

EOF

sed -n -e '/^MAC/,/^$/p' -e '/^BAG/,/^$/p' ${TMPFILE_PEM} | \
sed -e '/^[ 	]*Salt:/d' \
    -e '/^BAG #[0-9]*/,$ { /^[ 	]*Salt size:/d
}' > ${INFO_OUT}

diff ${INFO_EXP} ${INFO_OUT}

rc=$?

if test "${rc}" != "0"; then
	exit ${rc}
fi

rm -rf "${testdir}"

exit 0
