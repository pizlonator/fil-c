#!/bin/sh

# Copyright (C) 2019 Red Hat, Inc.
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
: ${builddir=.}
: ${SERV=../src/gnutls-serv${EXEEXT}}
: ${CLI=../src/gnutls-cli${EXEEXT}}
: ${CERTTOOL=../src/certtool${EXEEXT}}
: ${GREP=grep}
: ${DIFF=diff}
: ${SED=sed}
: ${CAT=cat}
TMPFILE_KEY=key.$$.pem.tmp
TMPFILE_CONFIG=config.$$.pem.tmp
TMPFILE_INPUT_SCRIPT=input.$$.script.tmp
TMPFILE_OBSERVED_LOG=observed.$$.log.tmp
TMPFILE_EXPECTED_LOG=expected.$$.log.tmp
export GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID=1

for tool in "${CERTTOOL}" "${SERV}" "${CLI}"; do
	if ! test -x "$tool"; then
		exit 77
	fi
done

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=15"
fi

if test "${WINDIR}" != ""; then
	exit 77
fi

. "${srcdir}/scripts/common.sh"

# This test doesn't work in FIPS mode
if test -n "${GNUTLS_FORCE_FIPS_MODE}" && test "${GNUTLS_FORCE_FIPS_MODE}" != 0; then
	exit 77
fi

cleanup() {
	rm -f "${TMPFILE_KEY}" "${TMPFILE_INPUT_SCRIPT}"
	rm -f "${TMPFILE_OBSERVED_LOG}" "${TMPFILE_EXPECTED_LOG}"
}
trap cleanup 1 15 2 EXIT

# Set up a reasonable but minimal configuration file using allowlisting
# allowing just a few curves.
# We intentionally add stray spaces and tabs to check our parser
cat <<_EOF_ > ${TMPFILE_CONFIG}
[global]
override-mode		= allowlist

[overrides]
  enabled-curve= seCp384r1  
enabled-curve =X448 
_EOF_
export GNUTLS_SYSTEM_PRIORITY_FILE="${TMPFILE_CONFIG}"
export GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID=1
export INITIALLY_ENABLED_CURVES="SECP384R1 X448"
export INITIALLY_DISABLED_CURVES="SECP256R1 SECP521R1 X25519"
EXAMPLE_DISABLED_PRIORITY=NORMAL:-CURVE-ALL:+CURVE-SECP256R1:+CURVE-SECP521R1:+CURVE-X25519

export GNUTLS_DEBUG_LEVEL=3


# --list output

"${CLI}" --list|grep ^Groups >"${TMPFILE_OBSERVED_LOG}"
cat "${TMPFILE_OBSERVED_LOG}"
for curve in ${INITIALLY_DISABLED_CURVES}; do
	if grep -i "$curve" "${TMPFILE_OBSERVED_LOG}"; then
		echo "Found disabled curve $curve within --list output"
		exit 1
	fi
done

for curve in ${INITIALLY_ENABLED_CURVES}; do
	if ! grep -i "$curve" ${TMPFILE_OBSERVED_LOG};then
		echo "Could not found $curve within --list output"
		exit 1
	fi
done


# TLS: try whether a client connection with a disabled curve will succeed

KEY1=${srcdir}/../doc/credentials/x509/key-rsa.pem
CERT1=${srcdir}/../doc/credentials/x509/cert-rsa.pem

unset GNUTLS_SYSTEM_PRIORITY_FILE

eval "${GETPORT}"
launch_server --echo --priority "NORMAL:-VERS-ALL:+VERS-TLS1.2:+VERS-TLS1.3" --x509keyfile ${KEY1} --x509certfile ${CERT1}
PID=$!
wait_server ${PID}

${VALGRIND} "${CLI}" -p "${PORT}" 127.0.0.1 --priority ${EXAMPLE_DISABLED_PRIORITY} --insecure --logfile ${TMPFILE_OBSERVED_LOG} </dev/null >/dev/null ||
	fail "expected connection to succeed (1)"

export GNUTLS_SYSTEM_PRIORITY_FILE="${TMPFILE_CONFIG}"

"${CLI}" -p "${PORT}" 127.0.0.1 --priority ${EXAMPLE_DISABLED_PRIORITY} --insecure --logfile ${TMPFILE_OBSERVED_LOG} </dev/null >/dev/null &&
	fail "expected connection to fail (2)"

kill ${PID}
wait

# TLS: try whether a server connection with a disabled curve will succeed

KEY1=${srcdir}/../doc/credentials/x509/key-rsa.pem
CERT1=${srcdir}/../doc/credentials/x509/cert-rsa.pem

eval "${GETPORT}"
launch_server --echo --priority "NORMAL" --x509keyfile ${KEY1} --x509certfile ${CERT1}
PID=$!
wait_server ${PID}

unset GNUTLS_SYSTEM_PRIORITY_FILE

${VALGRIND} "${CLI}" -p "${PORT}" 127.0.0.1 --priority ${EXAMPLE_DISABLED_PRIORITY} --insecure --logfile ${TMPFILE_OBSERVED_LOG} </dev/null >/dev/null &&
	fail "expected connection to fail (2)"

kill ${PID}
wait

export GNUTLS_SYSTEM_PRIORITY_FILE="${TMPFILE_CONFIG}"


# Key generation using certtool

for curve in ${INITIALLY_ENABLED_CURVES}; do
	key_type=ecdsa
	test $curve = X448 && key_type=x448
	test $curve = X25519 && key_type=x25519
	${VALGRIND} ${CERTTOOL} \
		--generate-privkey --key-type=$key_type --curve=$curve \
		--outfile "${TMPFILE_KEY}"
	EX=$?
	if test $EX != 0; then
		echo "key generation using $curve has failed ($EX)"
		exit $EX
	fi
done

for curve in ${INITIALLY_DISABLED_CURVES}; do
	key_type=ecdsa
	test $curve = X448 && key_type=x448
	test $curve = X25519 && key_type=x25519
	${VALGRIND} ${CERTTOOL} \
		--generate-privkey --key-type=$key_type --curve=$curve \
		--outfile "${TMPFILE_KEY}" > ${TMPFILE_OBSERVED_LOG} 2>&1
	EX=$?
	if test $EX != 1; then
		echo "key generation using $curve has succeeded unexpectedly"
		exit $EX
	fi
	if ! ${GREP} -Fqx "Unsupported curve: $curve" "${TMPFILE_OBSERVED_LOG}"
	then
		${CAT} "${TMPFILE_OBSERVED_LOG}"
		echo "'Unsupported curve: $curve' not found in the output"
		exit 1
	fi
done


# Test key generation and gnutls_ecc_curve_set_enabled
# using system-override-curves-allowlist.c

${CAT} > "${TMPFILE_EXPECTED_LOG}" <<EOF
disableable: SECP384R1
disableable: X448
reenableable: SECP256R1
reenableable: SECP521R1
reenableable: X25519
EOF

${VALGRIND} "${builddir}/system-override-curves-allowlist" \
	> ${TMPFILE_OBSERVED_LOG}
EX=$?
if test $EX != 0; then
	${CAT} ${TMPFILE_OBSERVED_LOG}
	echo "system-override-curves-allowlist(.c) failed with $EX"
	exit $EX
fi
${DIFF} "${TMPFILE_EXPECTED_LOG}" "${TMPFILE_OBSERVED_LOG}"
EX=$?
if test $EX != 0; then
	echo "system-override-curves-allowlist(.c) produced unexpected output"
	exit 1
fi

exit 0
