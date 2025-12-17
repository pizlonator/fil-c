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
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

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

: ${ac_cv_sizeof_time_t=8}
if test "${ac_cv_sizeof_time_t}" -ge 8; then
	ATTIME_VALID="2038-10-12"  # almost the pregenerated cert expiration
else
	ATTIME_VALID="2037-12-31"  # before 2038
fi


SERV="${SERV} -q"

. "${srcdir}/scripts/common.sh"

KEY1=${srcdir}/../doc/credentials/x509/key-rsa.pem
CERT1=${srcdir}/../doc/credentials/x509/cert-rsa.pem
CA1=${srcdir}/../doc/credentials/x509/ca.pem

ALLOWED_PARAMS="
rfc3526-group-14-2048
rfc3526-group-15-3072
rfc3526-group-16-4096
rfc3526-group-17-6144
rfc3526-group-18-8192
rfc7919-ffdhe2048
rfc7919-ffdhe3072
rfc7919-ffdhe4096
rfc7919-ffdhe6144
rfc7919-ffdhe8192
"

DISALLOWED_PARAMS="
rfc2409-group-2-1024
rfc3526-group-5-1536
rfc5054-1024
rfc5054-1536
rfc5054-2048
rfc5054-3072
rfc5054-4096
rfc5054-6144
rfc5054-8192
rfc5114-group-22-1024
rfc5114-group-23-2048
rfc5114-group-24-2048
"

OPTS="--priority=NORMAL:-VERS-ALL:+VERS-TLS1.2:-KX-ALL:+DHE-RSA:+AES-128-GCM:-GROUP-ALL"

for params in $ALLOWED_PARAMS; do
	echo "Checking with approved DH params: $params"

	PARAMS=${srcdir}/../doc/credentials/dhparams/${params}.pem

	eval "${GETPORT}"
	launch_server ${OPTS} --x509keyfile ${KEY1} --x509certfile ${CERT1} --dhparams ${PARAMS}
	PID=$!
	wait_server ${PID}

	${VALGRIND} "${CLI}" --attime "${ATTIME_VALID}" ${OPTS} -p "${PORT}" 127.0.0.1 --verify-hostname=localhost --x509cafile ${CA1} </dev/null >/dev/null || \
		fail ${PID} "handshake should have succeeded!"

	kill ${PID}
	wait
done

for params in $DISALLOWED_PARAMS; do
	echo "Checking with non-approved DH params: $params"

	PARAMS=${srcdir}/../doc/credentials/dhparams/${params}.pem

	eval "${GETPORT}"
	launch_server ${OPTS} --x509keyfile ${KEY1} --x509certfile ${CERT1} --dhparams ${PARAMS}
	PID=$!
	wait_server ${PID}

	${VALGRIND} "${CLI}" --attime "${ATTIME_VALID}" ${OPTS} -p "${PORT}" 127.0.0.1 --verify-hostname=localhost --x509cafile ${CA1} </dev/null >/dev/null

	RET=$?

	if test $RET -eq 0; then
		if test "${GNUTLS_FORCE_FIPS_MODE}" = 1; then
			fail ${PID} "handshake should have failed (FIPS mode 1)!"
		fi
	else
		if test "${GNUTLS_FORCE_FIPS_MODE}" != 1; then
			fail ${PID} "handshake should have succeeded (FIPS mode 0)!"
		fi
	fi

	kill ${PID}
	wait
done

exit 0
