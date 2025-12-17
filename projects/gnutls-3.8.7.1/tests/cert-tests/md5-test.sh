#!/bin/sh

# Copyright (C) 2006-2016 Free Software Foundation, Inc.
# Copyright (C) 2016 Red Hat, Inc.
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

: ${srcdir=.}
: ${CERTTOOL=../../src/certtool${EXEEXT}}
: ${DIFF=diff}
TMPFILE=md5.$$.tmp

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

export TZ="UTC"

. ${srcdir}/../scripts/common.sh

# Test MD5 signatures

"${CERTTOOL}" --attime "2016-04-15" --verify-chain --infile "${srcdir}/data/chain-md5.pem" >/dev/null 2>&1
rc=$?
if test "${rc}" != "1"; then
	echo "Test 1 (verification of RSA-MD5) failed"
	exit ${rc}
fi

"${CERTTOOL}" --attime "2016-04-15" --verify-allow-broken --verify-chain --infile "${srcdir}/data/chain-md5.pem" >/dev/null 2>&1
rc=$?
if test "${rc}" != "0"; then
	echo "Test 2 (verification of RSA-MD5 with allow-broken) failed"
	exit ${rc}
fi

rm -f "${TMPFILE}"

exit 0
