#!/bin/sh

# Copyright (C) 2015 Red Hat, Inc.
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

# This checks whether invalid UTF8 strings trigger valgrind warnings.

: ${srcdir=.}
: ${CERTTOOL=../../src/certtool${EXEEXT}}
: ${DIFF=diff}
if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND}"

	# Check improper UTF8 errors
	${VALGRIND} --error-exitcode=3 "${CERTTOOL}" -i --inder --infile "${srcdir}/data/cert-invalid-utf8.der"
	rc=$?

	if test "${rc}" = 3;then
		echo "Invalid memory access with invalid UTF8"
		exit 1
	fi
fi

exit 0
