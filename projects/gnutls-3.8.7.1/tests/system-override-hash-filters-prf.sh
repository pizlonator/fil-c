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

if ! test -x "${CLI}"; then
	exit 77
fi

TMPCFGFILE=cfg.$$.tmp
TMPOUTFILE=out.$$.tmp

# Sanity

${CLI} --list -d 4 --priority NORMAL > "${TMPOUTFILE}" 2>&1
if test $? != 0; then
	cat "${TMPOUTFILE}"
	echo 'fails just listing ciphersuites for NORMAL'
	exit 1
fi
if ! ${GREP} -Fq TLS_AES_256_GCM_SHA384 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'no TLS_AES_256_GCM_SHA384 (TLS 1.3) with NORMAL'
	exit 1
fi
if ! ${GREP} -Fq TLS_ECDHE_ECDSA_AES_256_GCM_SHA384 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'no TLS_ECDHE_ECDSA_AES_256_GCM_SHA384 (TLS 1.2) with NORMAL'
	exit 1
fi
if ! ${GREP} -q TLS_RSA_AES_128_GCM_SHA256 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'no TLS_RSA_AES_128_GCM_SHA256 (non-SHA384 example) with NORMAL'
	exit 1
fi

# insecure-hash = SHA384 disables TLS_AES_256_GCM_SHA384 and friends

cat <<_EOF_ > ${TMPCFGFILE}
[overrides]
insecure-hash = SHA384
_EOF_
export GNUTLS_SYSTEM_PRIORITY_FILE="${TMPCFGFILE}"
export GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID=1

${CLI} --list -d 4 --priority NORMAL > "${TMPOUTFILE}" 2>&1
if ${GREP} -Fq TLS_AES_256_GCM_SHA384 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'TLS_AES_256_GCM_SHA384 (TLS 1.3) has not disappeared'
	exit 1
fi
if ${GREP} -Fq TLS_ECDHE_ECDSA_AES_256_GCM_SHA384 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'TLS_ECDHE_ECDSA_AES_256_GCM_SHA384 (TLS 1.2) has not disappeared'
	exit 1
fi
if ! ${GREP} -q TLS_RSA_AES_128_GCM_SHA256 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'TLS_RSA_AES_128_GCM_SHA256 (non-SHA384 example) has disappeared'
	exit 1
fi
if ${GREP} -Fq SHA.*384 "${TMPOUTFILE}"; then
	cat "${TMPOUTFILE}"
	echo 'SHA384 is still mentioned'
	exit 1
fi

rm "${TMPCFGFILE}" "${TMPOUTFILE}"
