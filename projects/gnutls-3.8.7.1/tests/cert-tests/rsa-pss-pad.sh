#!/bin/sh

# Copyright (C) 2006-2012 Free Software Foundation, Inc.
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
TMPFILE=pss.$$.tmp

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

export TZ="UTC"

. ${srcdir}/../scripts/common.sh


# Test PSS signatures on certificate

for i in sha256 sha384 sha512;do
"${CERTTOOL}" --attime "2007-04-22" --generate-self-signed --key-type rsa-pss \
		--load-privkey "${srcdir}/data/privkey1.pem" \
		--template "${srcdir}/templates/template-test.tmpl" \
		--outfile "${TMPFILE}" --hash $i
rc=$?

if test -f "${srcdir}/data/template-rsa-$i.pem";then
${DIFF} "${srcdir}/data/template-rsa-$i.pem" "${TMPFILE}" >/dev/null 2>&1
rc=$?
fi

# We're done.
if test "${rc}" != "0"; then
	echo "Test (RSA-PSS-$i) failed"
	exit ${rc}
fi

"${CERTTOOL}" --attime "2007-04-25" --load-ca-certificate "${TMPFILE}" --verify --infile "${TMPFILE}" >/dev/null 2>&1
rc=$?
if test "${rc}" != "0"; then
	echo "Test (verification of RSA-PSS-$i) failed"
	exit ${rc}
fi
done

rm -f "${TMPFILE}"

exit 0
