#!/bin/sh

# Copyright (C) 2019 Nikos Mavrogiannopoulos
# Copyright (C) 2021 Red Hat, Inc.
#
# Author: Nikos Mavrogiannopoulos, Daiki Ueno
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

: ${builddir=.}
TMPFILE=c.$$.tmp
export GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID=1

cat <<_EOF_ > ${TMPFILE}
[overrides]

insecure-sig = rsa-pss-rsae-sha256
_EOF_

export GNUTLS_SYSTEM_PRIORITY_FILE="${TMPFILE}"

"${builddir}/system-override-sig-tls"
rc=$?
rm ${TMPFILE}
exit $rc
