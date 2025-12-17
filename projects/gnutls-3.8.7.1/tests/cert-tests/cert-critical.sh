#!/bin/sh

# Copyright (C) 2014 Nikos Mavrogiannopoulos
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
: ${DIFF=diff -b -B}

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND}"
fi

export TZ="UTC"

. ${srcdir}/../scripts/common.sh

${VALGRIND} "${CERTTOOL}" --attime "2017-2-28" --verify-chain --infile ${srcdir}/data/chain-with-critical-on-root.pem
rc=$?

if test "${rc}" != "1"; then
	echo "There was an issue verifying the chain"
	exit 1
fi

${VALGRIND} "${CERTTOOL}" --attime "2017-2-28" --verify-chain --infile ${srcdir}/data/chain-with-critical-on-endcert.pem
rc=$?

if test "${rc}" != "1"; then
	echo "There was an issue verifying the chain"
	exit 1
fi

${VALGRIND} "${CERTTOOL}" --attime "2017-2-28" --verify-chain --infile ${srcdir}/data/chain-with-critical-on-intermediate.pem
rc=$?

if test "${rc}" != "1"; then
	echo "There was an issue verifying the chain"
	exit 1
fi


exit 0
