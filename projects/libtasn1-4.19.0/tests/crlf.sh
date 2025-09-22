#!/bin/sh

# Copyright (C) 2006-2022 Free Software Foundation, Inc.
#
# Author: Simon Josefsson
#
# This file is part of LIBTASN1.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

set -e

srcdir=${srcdir:-.}

ASAN_OPTIONS="detect_leaks=0:exitcode=6"
export ASAN_OPTIONS

if ! test -z "${VALGRIND}";then
VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=7"
fi

ASN1DECODING=../src/asn1Decoding${EXEEXT}

# The crlf.cer file is a normal certificate which contains bytes 0x0A
# (LF), 0x0D (CF), and 0xFF (EOF), all are known to cause failures
# when using fopen/fgetc/fscanf on streams on some systems (read:
# Windows).

$VALGRIND $ASN1DECODING \
    $srcdir/../examples/pkix.asn \
    $srcdir/crlf.cer \
    PKIX1Implicit88.Certificate >/dev/null 2>&1
