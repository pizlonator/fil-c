#!/bin/sh

# Copyright (C) 2017 Nikos Mavrogiannopoulos
#
# Author: Nikos Mavrogiannopoulos
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
unset RETCODE

if ! test -x "${SERV}"; then
	exit 77
fi

if ! test -x "${CLI}"; then
	exit 77
fi

if test "${WINDIR}" != ""; then
	exit 77
fi

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=15"
fi


SERV="${SERV} -q"

. "${srcdir}/scripts/common.sh"

echo "Checking whether a client will refuse weak but trusted keys"

KEY1=${srcdir}/certs/rsa-512.pem
CERT1=${srcdir}/certs/rsa-512.pem

eval "${GETPORT}"
launch_server --echo --priority "NORMAL" --x509keyfile ${KEY1} --x509certfile ${CERT1}
PID=$!
wait_server ${PID}

"${CLI}" --attime "2019-12-20" -d 4 -p "${PORT}" localhost --x509cafile ${CERT1} --priority NORMAL:-VERS-TLS-ALL:+VERS-TLS1.2 </dev/null && \
	fail ${PID} "1. handshake with RSA should have failed!"

"${CLI}" --attime "2019-12-20" -d 4 -p "${PORT}" localhost --x509cafile ${CERT1} --priority NORMAL </dev/null && \
	fail ${PID} "2. handshake with RSA should have failed!"

kill ${PID}
wait

exit 0
