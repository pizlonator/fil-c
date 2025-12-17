#!/bin/sh

# Copyright (C) 2023 Red Hat, Inc.
#
# Author: Jakub Jelen <jjelen@redhat.com>
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

set -x

: ${srcdir=.}
: ${builddir=.}
: ${CERTTOOL=../src/certtool${EXEEXT}}
: ${P11TOOL=../src/p11tool${EXEEXT}}
: ${DIFF=diff}

if test "${GNUTLS_FORCE_FIPS_MODE}" = 1;then
	echo "Cannot run in FIPS140-2 mode"
	exit 77
fi

. "$srcdir/scripts/common.sh"

testdir=`create_testdir pkcs11-tool`

TMP_SOFTHSM_DIR="$testdir/softhsm-load.$$.tmp"
TEMPLATE="$testdir/cert.cfg"
PIN=1234
PUK=1234

if ! test -x "${P11TOOL}"; then
	exit 77
fi

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

for lib in ${libdir} ${libdir}/pkcs11 /usr/lib64/pkcs11/ /usr/lib/pkcs11/ /usr/lib/x86_64-linux-gnu/pkcs11/ /usr/lib/softhsm/; do
	if test -f "${lib}/libsofthsm2.so"; then
		SOFTHSM_MODULE="${lib}/libsofthsm2.so"
		echo "located ${MODULE}"
		break
	fi
done

if ! test -f "${SOFTHSM_MODULE}"; then
	echo "softhsm module was not found"
	exit 77
fi

if [ -z "$(which pkcs11-tool 2>/dev/null)" ]; then
	echo "Need pkcs11-tool from opensc package to run this test."
	exit 77
fi

# Setup softhsm
rm -rf ${TMP_SOFTHSM_DIR}
mkdir -p ${TMP_SOFTHSM_DIR}
SOFTHSM2_CONF=${TMP_SOFTHSM_DIR}/conf
export SOFTHSM2_CONF
echo "objectstore.backend = file" > "${SOFTHSM2_CONF}"
echo "directories.tokendir = ${TMP_SOFTHSM_DIR}" >> "${SOFTHSM2_CONF}"

softhsm2-util --init-token --slot 0 --label "GnuTLS-Test" --so-pin "${PUK}" --pin "${PIN}" >/dev/null #2>&1
if test $? != 0; then
	echo "failed to initialize softhsm"
	exit 1
fi

# Reproducer for 
# https://gitlab.com/gnutls/gnutls/-/issues/1515

# Generate Ed25519 key using pkcs11-tool
LABEL="Ed25519key"
ID="01"
pkcs11-tool --keypairgen --key-type="EC:edwards25519" --login --pin="$PIN" --module="$SOFTHSM_MODULE" --label="$LABEL" --id="$ID"
if test $? != 0; then
	echo "failed to generate ed25519 key pair"
	exit 1
fi

# check p11tool can read these keys
${P11TOOL} --list-all --login --set-pin="$PIN" --provider="$SOFTHSM_MODULE" pkcs11:
if test $? != 0; then
	echo "failed to generate list generated keys using p11tool"
	exit 1
fi

cat <<_EOF_ >${TEMPLATE}
cn = test
ca
cert_signing_key
expiration_days = 1
_EOF_

GNUTLS_PIN="$PIN" ${CERTTOOL} --generate-self-signed --outfile="$testdir/ed25519-ca.crt" \
    --template=${TEMPLATE} --provider="$SOFTHSM_MODULE" \
    --load-privkey "pkcs11:object=$LABEL;type=private" \
    --load-pubkey "pkcs11:object=$LABEL;type=public" --outder
if test $? != 0; then
	echo "failed to self-sign the ed25519 key"
	exit 1
fi

# TODO add test when opensc will support Ed448
# Generate Ed448 key using pkcs11-tool
#pkcs11-tool --keypairgen --key-type="EC:edwards448" --login --pin="$PIN" --module="$SOFTHSM_MODULE" --label="Ed448 key" --id=02
#if test $? != 0; then
#	echo "failed to generate ed448 key pair"
#	exit 1
#fi


rm -rf "$testdir"
exit 0
