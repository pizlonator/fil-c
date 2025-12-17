#!/bin/sh

# Copyright (C) 2022 Red Hat, Inc.
#
# Author: Daiki Ueno
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

. "$srcdir/scripts/common.sh"

if ! grep '^tls ' /proc/modules 2>&1 /dev/null; then
    exit 77
fi

testdir=`create_testdir ktls_keyupdate`

cfg="$testdir/config"

cat <<EOF > "$cfg"
[global]
ktls = true
EOF

GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID=1 \
GNUTLS_SYSTEM_PRIORITY_FILE="$cfg" \
"$builddir/ktls_keyupdate" "$@"
rc=$?

rm -rf "$testdir"
exit $rc
