#!/bin/sh

# Copyright (C) 2019 Nikos Mavrogiannopoulos
#
# Author: Nikos Mavrogiannopoulos
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
[global]
override-mode = allowlist

[overrides]
secure-hash = sha256
secure-sig = rsa-sha256
secure-hash = sha384
secure-sig = rsa-pss-sha384
_EOF_

export GNUTLS_SYSTEM_PRIORITY_FILE="${TMPFILE}"

"${builddir}/system-override-sig"
rc=$?
rm ${TMPFILE}
exit $rc
