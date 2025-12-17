#!/bin/sh

# Copyright (C) 2021 Free Software Foundation, Inc.
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
OUTCERT="policy-cert.$$.tmp"

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

export TZ="UTC"

. ${srcdir}/../scripts/common.sh

"${CERTTOOL}" --attime "2007-04-22" --generate-self-signed \
	--load-privkey "${srcdir}/data/template-test.key" \
	--template "${srcdir}/templates/simple-policy.tmpl" \
	--outfile $OUTCERT #2>/dev/null

${DIFF} "${srcdir}/data/simple-policy.pem" $OUTCERT #>/dev/null 2>&1
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "Test with simple policy failed"
	exit ${rc}
fi

rm -f "$OUTCERT"

exit 0
