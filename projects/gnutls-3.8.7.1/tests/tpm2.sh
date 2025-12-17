#!/bin/sh

# Copyright (C) 2018-2019 IBM Corporation
# Copyright (C) 2019,2021 Red Hat, Inc.
#
# Author: Stefan Berger, Nikos Mavrogiannopoulos, Daiki Ueno
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
: ${CERTTOOL=../src/certtool${EXEEXT}}
KEYPEMFILE=tpmkey.$$.key.pem
CTXFILE=tpmkey.$$.ctx

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

if [ -z "$(which swtpm 2>/dev/null)" ]; then
	echo "Need swtpm package to run this test."
	exit 77
fi

if [ -z "$(which ncat 2>/dev/null)" ]; then
	echo "Need ncat from nmap-ncat package to run this test."
	exit 77
fi

if [ -z "$(which tpm2_startup 2>/dev/null)" ]; then
	echo "Need tpm2_startup from tpm2-tools package to run this test."
	exit 77
fi

if [ -z "$(which base64 2>/dev/null)" ]; then
	echo "Need the base64 tool to run this test."
	exit 77
fi

: ${OPENSSL=openssl}

case `"$OPENSSL" version` in
    *OpenSSL\ 3*)
	echo "This test is not yet compatible with OpenSSL 3."
	exit 77
	;;
esac

if [ -z "$(which tpm2tss-genkey 2>/dev/null)" ]; then
	echo "Need tpm2tss-genkey from tpm2-tss-engine package to run this test."
	exit 77
fi

. "${srcdir}/scripts/common.sh"

workdir=$(mktemp -d)

PORT=2321
SWTPM_SERVER_PORT=$PORT
echo "Server port: $PORT"
SWTPM_CTRL_PORT=$((SWTPM_SERVER_PORT + 1)) # fake port used by ncat only
echo "Ncat port: $SWTPM_CTRL_PORT"
echo "Directory: $workdir"

SWTPM_PIDFILE=${workdir}/swtpm.pid

eval "${GETPORT}"

TCSD_LISTEN_PORT=$PORT
export TSS_TCSD_PORT=$TCSD_LISTEN_PORT
echo "TCSD port: $PORT"

export TPM2TOOLS_TCTI="mssim:host=127.0.0.1,port=${SWTPM_SERVER_PORT}"
export TPM2TSSENGINE_TCTI="$TPM2TOOLS_TCTI"
export TPM20TEST_TCTI_NAME="socket"
export TPM20TEST_SOCKET_PORT=${SWTPM_SERVER_PORT}
export TPM20TEST_SOCKET_ADDRESS="127.0.0.1"

cleanup()
{
	echo "Cleaning up"
	stop_swtpm
	rm -f ${KEYPEMFILE}
	if [ -n "$workdir" ]; then
		rm -rf $workdir
	fi
}

start_swtpm()
{
	local workdir="$1"

	local res

	echo ""
	echo " - Starting swtpm"

	swtpm socket \
		--tpm2 \
		--flags not-need-init \
		--pid file=$SWTPM_PIDFILE \
		--tpmstate dir=$workdir \
		--server type=tcp,bindaddr=127.0.0.1,port=$SWTPM_SERVER_PORT &

	if wait_for_file $SWTPM_PIDFILE 3; then
		echo "Starting the swtpm failed"
		return 1
	fi

	echo " - Starting ncat"

	SWTPM_PID=$(cat $SWTPM_PIDFILE)
	kill -0 ${SWTPM_PID}
	if [ $? -ne 0 ]; then
		echo "swtpm must have terminated"
		return 1
	fi

	ncat -l ${SWTPM_CTRL_PORT} \
         -k -c "xargs --null -n1 printf '\x00\x00\x00\x00'" &>/dev/null &
	if [ $? -ne 0 ]; then
		echo "Could not start ncat"
		stop_swtpm
		return 1
	fi
	NCAT_PID=$!
	sleep 1
	kill -0 ${NCAT_PID}
	if [ $? -ne 0 ]; then
		echo "ncat must have been terminated"
		stop_swtpm
		return 1
	fi

	echo " - Running tpm2_startup"
	msg=$(tpm2_startup -V -c 2>&1)
	if [ $? -ne 0 ]; then
		echo "TPM2_Startup() failed"
		echo "${msg}"
		stop_swtpm
		return 1
	fi

	echo " - Startup completed"
	sleep 1

	return 0
}

stop_swtpm()
{
	if [ -n "${SWTPM_PID}" ]; then
		echo terminate_proc ${SWTPM_PID}
		terminate_proc ${SWTPM_PID}
		unset SWTPM_PID
	fi

	if [ -n "${NCAT_PID}" ]; then
		terminate_proc ${NCAT_PID}
		unset NCAT_PID
	fi
}

run_tests()
{
	local workdir="$1"
	local OPASS=12345678
	local EPASS=23456789
	local LPASS=34567890
#	local OBJPASS=012345
	local kalg=$2

	[ -z "$workdir" ] && {
		echo "No workdir"
		return 1
	}

	start_swtpm $workdir

	echo " - Set owner authorization"
	tpm2_changeauth -c owner ${OPASS}
	echo " - Set endorsement authorization"
	tpm2_changeauth -c endorsement ${EPASS}
	echo " - Set lockout authorization"
	tpm2_changeauth -c lockout ${LPASS}

	echo " - Generating ${KEYPEMFILE}"
	tpm2tss-genkey -a ${kalg} -o ${OPASS} ${KEYPEMFILE}
	if [ $? -ne 0 ]; then
		echo "unable to generate key"
		return 1
	fi
	cat ${KEYPEMFILE}

	echo " - Generating certificate based on key"

	export GNUTLS_PIN=${OPASS}
	"${CERTTOOL}" --generate-self-signed -d 3 \
		--load-privkey "${KEYPEMFILE}" \
		--template "${srcdir}/cert-tests/templates/template-test.tmpl"
	if [ $? -ne 0 ]; then
		echo "unable to generate certificate"
		return 1
	fi

	if test "${kalg}" = "rsa";then
		echo " - Generating RSA-PSS certificate based on key"
		"${CERTTOOL}" --generate-self-signed -d 3 \
			--load-privkey "${KEYPEMFILE}" \
			--sign-params rsa-pss \
			--template "${srcdir}/cert-tests/templates/template-test.tmpl"
		if [ $? -ne 0 ]; then
			echo "unable to generate certificate"
			return 1
		fi
	fi

	stop_swtpm
	echo "Ok"

	return 0
}

trap "cleanup" EXIT QUIT

run_tests "$workdir" ecdsa
run_tests "$workdir" rsa
