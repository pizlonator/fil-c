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
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

: ${top_builddir=..}
: ${CLI_DEBUG=../src/gnutls-cli-debug${EXEEXT}}
: ${LDD=ldd}
: ${LIBTOOL=libtool}

if ! test -x "${CLI_DEBUG}"; then
	exit 77
fi

# ldd.sh doesn't check recursive dependencies
${LDD} --version >/dev/null || exit 77

# We use gnutls-cli-debug, as it has the fewest dependencies among our
# commands (e.g., gnutls-cli pulls in OpenSSL through libunbound).
if ${LIBTOOL} --mode=execute ${LDD} ${CLI_DEBUG} | \
    grep '^[[:space:]]*\(libcrypto\.\|libssl\.\|libgcrypt\.\)'; then
    echo "gnutls-cli-debug links to other crypto library"
    exit 1
fi
