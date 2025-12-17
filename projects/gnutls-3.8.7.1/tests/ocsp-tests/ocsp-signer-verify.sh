#!/bin/sh

# Copyright (C) 2021 Fiona Klute
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
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

: ${srcdir=.}
: ${OCSPTOOL=../src/ocsptool${EXEEXT}}
: ${DIFF=diff}

if ! test -x "${OCSPTOOL}"; then
    exit 77
fi

export TZ="UTC"

. "${srcdir}/scripts/common.sh"

date="2021-07-14 00:00:00"
sample_dir="${srcdir}/ocsp-tests/signer-verify"
trusted="${sample_dir}/trust.pem"

verify_response ()
{
    echo "verifying ${sample_dir}/${1} using ${trusted}"
    "${OCSPTOOL}" --attime "${date}" --infile="${sample_dir}/${1}" \
              --verify-response --load-trust="${trusted}"
    return $?
}

if ! verify_response response-ca.der; then
    echo "verification of OCSP response signature by CA failed"
    exit 1
fi

if ! verify_response response-delegated.der; then
    echo "verification of OCSP response signature by delegated signer failed"
    exit 1
fi

if verify_response response-non-delegated.der; then
    echo "verification of OCSP response signature by non-signer certificate " \
         "from the same CA succeeded, but should have failed"
    exit 1
fi
