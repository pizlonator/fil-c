#!/bin/sh

# Copyright (C) 2022 Red Hat, Inc.
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GnuTLS. If not, see <https://www.gnu.org/licenses/>.

: ${srcdir=.}
: ${SERV=../src/gnutls-serv${EXEEXT}}
: ${CLI=../src/gnutls-cli${EXEEXT}}

if ! test -x "${SERV}"; then
	exit 77
fi

if ! test -x "${CLI}"; then
	exit 77
fi

. "${srcdir}/scripts/common.sh"

if ! "${CLI}" --list | grep '^Groups: .*GROUP-X25519-KYBER768.*' >/dev/null; then
    if "${CLI}" --list | grep '^Public Key Systems: .*KYBER768.*' >/dev/null; then
	fail "KYBER768 is in Public Key Systems, while GROUP-X25519-KYBER768 is NOT in Groups"
    fi
    exit 77
else
    if ! "${CLI}" --list | grep '^Public Key Systems: .*KYBER768.*' >/dev/null; then
	fail "KYBER768 is NOT in Public Key Systems, while GROUP-X25519-KYBER768 is in Groups"
    fi
fi

testdir=`create_testdir pqc-hybrid-kx`

KEY="$srcdir/../doc/credentials/x509/key-ecc.pem"
CERT="$srcdir/../doc/credentials/x509/cert-ecc.pem"
CACERT="$srcdir/../doc/credentials/x509/ca.pem"

eval "${GETPORT}"
launch_server --echo --priority NORMAL:-GROUP-ALL:+GROUP-X25519-KYBER768 --x509keyfile="$KEY" --x509certfile="$CERT"
PID=$!
wait_server ${PID}

${VALGRIND} "${CLI}" -p "${PORT}" localhost --priority NORMAL:-GROUP-ALL:+GROUP-X25519-KYBER768 --x509cafile="$CACERT" --logfile="$testdir/cli.log" </dev/null

kill ${PID}
wait

grep -- '- Description: (TLS1.3-X.509)-(ECDHE-X25519-KYBER768)-(ECDSA-SECP256R1-SHA256)-(AES-256-GCM)' "$testdir/cli.log" || { echo "unexpected handshake description"; exit 1; }

rm -rf "$testdir"
exit 0
