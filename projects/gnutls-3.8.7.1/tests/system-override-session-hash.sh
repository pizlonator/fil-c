#!/bin/sh

# Copyright (C) 2021 Red Hat, Inc.
#
# Author: Alexander Sosedkin
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
: ${SERV=../src/gnutls-serv${EXEEXT}}
: ${CLI=../src/gnutls-cli${EXEEXT}}

if ! test -x "${SERV}"; then
	exit 77
fi

if ! test -x "${CLI}"; then
	exit 77
fi

${CLI} --fips140-mode
if test $? = 0;then
	echo "Cannot run this test in FIPS140 mode"
	exit 77
fi

. "${srcdir}/scripts/common.sh"

testdir=`create_testdir system-override-session-hash`

cat <<_EOF_ > "$testdir/request.cfg"
[overrides]

tls-session-hash = request
_EOF_

cat <<_EOF_ > "$testdir/require.cfg"
[overrides]

tls-session-hash = require
_EOF_

eval "${GETPORT}"

KEY=${srcdir}/../doc/credentials/x509/key-rsa-pss.pem
CERT=${srcdir}/../doc/credentials/x509/cert-rsa-pss.pem
CA=${srcdir}/../doc/credentials/x509/ca.pem

unset GNUTLS_SYSTEM_PRIORITY_FILE
unset GNUTLS_DEBUG_LEVEL

launch_server --echo --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2" --x509keyfile ${KEY} --x509certfile ${CERT}
PID=$!
wait_server ${PID}

export GNUTLS_SYSTEM_PRIORITY_FILE="$testdir/request.cfg"
export GNUTLS_DEBUG_LEVEL=3

"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail "expected connection to succeed (1)"

# "tls-session-hash" has precedence over %FORCE_SESSION_HASH
"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2:%FORCE_SESSION_HASH" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail "expected connection to succeed (2)"

echo kill ${PID}
kill ${PID}
wait

unset GNUTLS_SYSTEM_PRIORITY_FILE
unset GNUTLS_DEBUG_LEVEL

export GNUTLS_SYSTEM_PRIORITY_FILE="$testdir/request.cfg"

# "tls-session-hash" has precedence over %FORCE_SESSION_HASH
launch_server --echo --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2:%FORCE_SESSION_HASH" --x509keyfile ${KEY} --x509certfile ${CERT}
PID=$!
wait_server ${PID}

export GNUTLS_DEBUG_LEVEL=3

"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail ${PID} "expected connection to succeed (3)"

"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2:%NO_SESSION_HASH" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail ${PID} "expected connection to succeed (4)"

kill ${PID}
wait

unset GNUTLS_SYSTEM_PRIORITY_FILE
unset GNUTLS_DEBUG_LEVEL

launch_server --echo --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2" --x509keyfile ${KEY} --x509certfile ${CERT}
PID=$!
wait_server ${PID}

export GNUTLS_SYSTEM_PRIORITY_FILE="$testdir/require.cfg"
export GNUTLS_DEBUG_LEVEL=3

"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail ${PID} "expected connection to succeed (5)"

# "tls-session-hash" has precedence over %NO_SESSION_HASH
"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2:%NO_SESSION_HASH" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail ${PID} "expected connection to succeed (6)"

kill ${PID}
wait

unset GNUTLS_SYSTEM_PRIORITY_FILE
unset GNUTLS_DEBUG_LEVEL

export GNUTLS_SYSTEM_PRIORITY_FILE="$testdir/require.cfg"

# "tls-session-hash" has precedence over %NO_SESSION_HASH
launch_server --echo --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2:%NO_SESSION_HASH" --x509keyfile ${KEY} --x509certfile ${CERT}
PID=$!
wait_server ${PID}

"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail ${PID} "expected connection to succeed (7)"

# "tls-session-hash" has precedence over %NO_SESSION_HASH
"${CLI}" -p "${PORT}" 127.0.0.1 --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2:%NO_SESSION_HASH" --verify-hostname=localhost --x509cafile ${CA} --logfile="$testdir/client.log" </dev/null >/dev/null ||
	fail ${PID} "expected connection to succeed (8)"

kill ${PID}
wait

rm -rf "$testdir"
