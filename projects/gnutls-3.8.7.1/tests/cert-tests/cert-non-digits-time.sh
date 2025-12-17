#!/bin/sh

# Copyright (C) 2017 Red Hat, Inc.
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

# Check whether certificates with non-digits time fields are accepted
${VALGRIND}"${CERTTOOL}" --attime "2019-12-19" --verify --load-ca-certificate "${srcdir}/data/cert-with-non-digits-time-ca.pem" --infile "${srcdir}/data/cert-with-non-digits-time.pem"
rc=$?

if test "${rc}" = "0";then
    echo "certificate whose notbefore field is a non-digits was accepted"
    exit 1
fi

exit 0
