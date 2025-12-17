#!/bin/sh

# Copyright (C) 2016 Nikos Mavrogiannopoulos
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
: ${OCSPTOOL=../src/ocsptool${EXEEXT}}
: ${DIFF=diff}

if ! test -x "${OCSPTOOL}"; then
	exit 77
fi

export TZ="UTC"

. "${srcdir}/scripts/common.sh"

"${OCSPTOOL}" --attime "2016-04-22" -e --load-signer "${srcdir}/ocsp-tests/certs/ca.pem" --infile "${srcdir}/ocsp-tests/response1.der"
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "Test 1 - OCSP signed by CA - failed"
	exit ${rc}
fi

"${OCSPTOOL}" --attime "2016-04-22" -e --load-signer "${srcdir}/ocsp-tests/certs/ocsp-server.pem" --infile "${srcdir}/ocsp-tests/response2.der"
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "Test 2 - OCSP signed by delegated cert - failed"
	exit ${rc}
fi

"${OCSPTOOL}" --attime "2016-04-22" -e --load-signer "${srcdir}/ocsp-tests/certs/ca.pem" --infile "${srcdir}/ocsp-tests/response2.der" -d 4
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "Test 3 - OCSP signed by delegated cert - failed"
	exit ${rc}
fi


exit 0
