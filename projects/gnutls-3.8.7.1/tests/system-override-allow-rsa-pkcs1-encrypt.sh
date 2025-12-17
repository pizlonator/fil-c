#!/bin/sh

# Copyright (C) 2024 Red Hat, Inc.
#
# Author: Zoltan Fridrich
#
# This file is part of GnuTLS.
#
# GnuTLS is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# GnuTLS is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

TEST=${builddir}/rsaes-pkcs1-v1_5
CONF=config.$$.tmp
export GNUTLS_SYSTEM_PRIORITY_FILE=${CONF}
export GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID=1

if test "${WINDIR}" != ""; then
	exit 77
fi

if test "${GNUTLS_FORCE_FIPS_MODE}" = 1; then
	exit 77
fi

cat <<_EOF_ > ${CONF}
[overrides]
allow-rsa-pkcs1-encrypt = true
_EOF_

${TEST}
if [ $? != 0 ]; then
	echo "${TEST} expected to succeed"
	exit 1
fi
echo "RSAES-PKCS1-v1_5 successfully enabled"

cat <<_EOF_ > ${CONF}
[overrides]
allow-rsa-pkcs1-encrypt = false
_EOF_

${TEST}
if [ $? = 0 ]; then
	echo "${TEST} expected to fail"
	exit 1
fi
echo "RSAES-PKCS1-v1_5 successfully disabled"

unset GNUTLS_SYSTEM_PRIORITY_FILE
unset GNUTLS_SYSTEM_PRIORITY_FAIL_ON_INVALID

${TEST}
if [ $? != 0 ]; then
	echo "${TEST} expected to succeed by default"
	exit 1
fi
echo "RSAES-PKCS1-v1_5 successfully enabled by default"

exit 0
