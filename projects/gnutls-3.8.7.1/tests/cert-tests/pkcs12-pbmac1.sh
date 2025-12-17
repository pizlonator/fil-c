#!/bin/sh

# Copyright (C) 2004-2006, 2008, 2010, 2012, 2024 Free Software Foundation,
# Inc.
#
# Author: Daiki Ueno
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

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=1"
fi

: ${DIFF=diff}
DEBUG=""

. "${srcdir}/../scripts/common.sh"
testdir=`create_testdir pkcs12-pbmac1`

TMPFILE=$testdir/pkcs12
TMPFILE_PEM=$testdir/pkcs12.pem

DEBUG="1"

GOOD="
pbmac1_256_256.good.p12
pbmac1_512_256.good.p12
pbmac1_512_512.good.p12
pbmac1-simple.p12
"

BAD="
pbmac1_256_256.bad-iter.p12
pbmac1_256_256.bad-salt.p12
pbmac1_256_256.no-len.p12
pbmac1_256_256.short-len.p12
pbmac1_256_256.extended-mac.p12
pbmac1_256_256.truncated-len.p12
"

for p12 in $GOOD; do
	set -- ${p12}
	file="$1"

	if test "x$DEBUG" != "x"; then
		${VALGRIND} "${CERTTOOL}" -d 99 --p12-info --inder --password 1234 \
			--infile "${srcdir}/data/${file}"
	else
		${VALGRIND} "${CERTTOOL}" --p12-info --inder --password 1234 \
			--infile "${srcdir}/data/${file}" >/dev/null
	fi
	rc=$?
	if test ${rc} != 0; then
		echo "PKCS12 FATAL ${p12}"
		exit 1
	fi
done

for p12 in $BAD; do
	set -- ${p12}
	file="$1"

	if test "x$DEBUG" != "x"; then
		${VALGRIND} "${CERTTOOL}" -d 99 --p12-info --inder --password 1234 \
			--infile "${srcdir}/data/${file}"
	else
		${VALGRIND} "${CERTTOOL}" --p12-info --inder --password 1234 \
			--infile "${srcdir}/data/${file}" >/dev/null
	fi
	rc=$?
	if test ${rc} = 0; then
		echo "PKCS12 FATAL ${p12}"
		exit 1
	fi
done

# test whether we can encode a certificate and a key
${VALGRIND} "${CERTTOOL}" --to-p12 --pbmac1 --password 1234 --p12-name "my-key" --load-certificate "${srcdir}/../certs/cert-ecc256.pem" --load-privkey "${srcdir}/../certs/ecc256.pem" --outder --outfile $TMPFILE >/dev/null
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

# check if PBMAC1 is used by default in FIPS mode
if test "$GNUTLS_FORCE_FIPS_MODE" = 1; then
	${VALGRIND} "$CERTTOOL" --to-p12 --password 1234 --p12-name "my-key" --load-certificate "$srcdir/../certs/cert-ecc256.pem" --load-privkey "$srcdir/../certs/ecc256.pem" --outder --outfile "$TMPFILE" >/dev/null
	rc=$?
	if test $rc != 0; then
		echo "PKCS12 FATAL encoding"
		exit 1
	fi
	${VALGRIND} "$CERTTOOL" -d 99 --p12-info --inder --password 1234 \
		    --infile "$TMPFILE" | grep "^	MAC: PBMAC1" || {
		echo "Generated PKCS12 file doesn't use PBMAC1 in FIPS mode"
		exit 1
	}
fi

rm -rf "${testdir}"

exit 0
