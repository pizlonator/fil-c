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
: ${CLI=../src/gnutls-cli${EXEEXT}}
: ${GREP=grep}
: ${DIFF=diff}
: ${SED=sed}

if ! test -x "${CLI}"; then
	exit 77
fi

TMPCFGFILE=cfg.$$.tmp
TMPREFFILE=ref.$$.tmp
TMPCMPFILE=cmp.$$.tmp
TMPOUTFILE=out.$$.tmp
TMPSPECIAL=spc.$$.tmp

# extract the list of %SPECIALs from the sources

< ${srcdir}/../lib/priority_options.gperf \
	${SED} -ne '/\([A-Z_0-9]\{1,\}\), .*/p' | \
	${SED} -e 's/\([A-Z_0-9]\{1,\}\), .*/\1/' > "${TMPSPECIAL}"

if ! ${GREP} -Fqx STATELESS_COMPRESSION "${TMPSPECIAL}"; then
	cat "${TMPSPECIAL}"
	echo 'source-extracted list of %SPECIALs has no %STATELESS_COMPRESSION'
	exit 1
fi

# Set up a configuration file using allowlisting
# allowing for both TLS 1.2 and TLS 1.3
# (so that %NO_EXTENSIONS later caps that just TLS 1.2)

cat <<_EOF_ > ${TMPCFGFILE}
[global]
override-mode = allowlist

[overrides]
secure-hash = SHA256
tls-enabled-mac = AEAD
tls-enabled-group = GROUP-FFDHE3072
secure-sig = RSA-SHA256
tls-enabled-cipher = AES-128-GCM
tls-enabled-kx = RSA
enabled-version = TLS1.3
enabled-version = TLS1.2
_EOF_
export GNUTLS_SYSTEM_PRIORITY_FILE="${TMPCFGFILE}"
export GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID=1

# Smoke --list, @SYSTEM

${CLI} --list -d 4 --priority @SYSTEM > "${TMPOUTFILE}" 2>&1
if test $? != 0; then
	cat "${TMPOUTFILE}"
	echo 'fails with just @SYSTEM'
	exit 1
fi
if ! ${GREP} -Fqx 'Protocols: VERS-TLS1.3, VERS-TLS1.2' \
		"${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'unexpected protocol list with @SYSTEM'
	exit 1
fi
if ! ${GREP} -Fq TLS_AES_128_GCM_SHA256 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'no TLS_AES_128_GCM_SHA256 with just @SYSTEM'
	exit 1
fi
if ! ${GREP} -q TLS_RSA_AES_128_GCM_SHA256 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'no TLS_RSA_AES_128_GCM_SHA256 with just @SYSTEM'
	exit 1
fi
${SED} 's/for @SYSTEM/for ---PRIORITY---/' "${TMPOUTFILE}" > "${TMPREFFILE}"

# Smoke-test a no-op %STATELESS_COMPRESSION, expect --list to stay the same

${CLI} --list -d 4 --priority @SYSTEM:%STATELESS_COMPRESSION > "${TMPOUTFILE}" 2>&1
if test $? != 0; then
	cat "${TMPOUTFILE}"
	echo 'fails with %STATELESS_COMPRESSION'
	exit 1
fi
${SED} 's/for @SYSTEM:%STATELESS_COMPRESSION/for ---PRIORITY---/' \
	"${TMPOUTFILE}" > "${TMPCMPFILE}"
if ! ${DIFF} "${TMPCMPFILE}" "${TMPREFFILE}"; then
	echo '%STATELESS_COMPRESSION has changed the output'
	exit 1
fi

# Smoke-test %NONEXISTING_OPTION, expect a syntax error

${CLI} --list -d 4 --priority @SYSTEM:%NONEXISTING_OPTION > "${TMPOUTFILE}" 2>&1
if test $? = 0; then
	cat "${TMPOUTFILE}"
	echo 'unknown option was not caught'
	exit 1
fi
if ! ${GREP} -Fq 'Syntax error at: @SYSTEM:%NONEXISTING_OPTION' "${TMPOUTFILE}"
then
	cat "${TMPOUTFILE}"
	echo 'unknown option was not errored upon'
	exit 1
fi

# Test impact-less %SPECIALs, expect --list to stay the same

while read special; do
	if test "$special" = NO_EXTENSIONS; then
		continue  # see below
	fi
	prio="@SYSTEM:%$special"
	${CLI} --list -d 4 --priority "$prio" > "${TMPOUTFILE}" 2>&1
	if test $? != 0; then
		cat "${TMPOUTFILE}"
		echo "fails with $prio"
		exit 1
	fi
	${SED} "s/for $prio/for ---PRIORITY---/" "${TMPOUTFILE}" \
		> "${TMPCMPFILE}"
	if ! ${DIFF} "${TMPCMPFILE}" "${TMPREFFILE}"; then
		echo "$special has changed the output"
		exit 1
	fi
done < "${TMPSPECIAL}"

# Check that %NO_EXTENSIONS changes the output, capping it to TLS 1.2

${CLI} --list -d 4 --priority @SYSTEM:%NO_EXTENSIONS > "${TMPOUTFILE}" 2>&1
if test $? != 0; then
	cat "${TMPOUTFILE}"
	echo 'fails with just @SYSTEM'
	exit 1
fi
if ! ${GREP} -Fqx 'Protocols: VERS-TLS1.2' \
		"${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'unexpected protocol list with @SYSTEM:%NO_EXTENSIONS'
	exit 1
fi
if ${GREP} -Fq TLS_AES_128_GCM_SHA256 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'TLS_AES_128_GCM_SHA256 present with @SYSTEM:%NO_EXTENSIONS'
	exit 1
fi
if ! ${GREP} -q TLS_RSA_AES_128_GCM_SHA256 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'no TLS_RSA_AES_128_GCM_SHA256 with @SYSTEM:%NO_EXTENSIONS'
	exit 1
fi

rm "${TMPCFGFILE}" "${TMPREFFILE}" "${TMPCMPFILE}" "${TMPOUTFILE}"
rm "${TMPSPECIAL}"

exit 0
