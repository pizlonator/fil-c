# Copyright (C) 2014 Red Hat, Inc.
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

unset RETCODE
if ! test -z "${VALGRIND}"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND}"
fi

: ${srcdir=.}
. "${srcdir}/../scripts/common.sh"

echo default cipher tests
${PROG}
ret=$?
if test $ret != 0; then
	echo "default cipher tests failed"
	exit $ret
fi

echo all optimizations disabled
GNUTLS_CPUID_OVERRIDE=0x1 ${PROG}
ret=$?
if test $ret != 0; then
	echo "included cipher tests failed"
	exit $ret
fi

exit_if_non_x86

echo AESNI
GNUTLS_CPUID_OVERRIDE=0x2 ${PROG}
ret=$?
if test $ret != 0; then
	echo "AESNI cipher tests failed"
	exit $ret
fi

echo SSEE3
GNUTLS_CPUID_OVERRIDE=0x4 ${PROG}
ret=$?
if test $ret != 0; then
	echo "SSSE3 cipher tests failed"
	exit $ret
fi

echo AESNI+PCLMUL
GNUTLS_CPUID_OVERRIDE=0xA ${PROG}
ret=$?
if test $ret != 0; then
	echo "PCLMUL cipher tests failed"
	exit $ret
fi

echo AESNI+PCLMUL+AVX
GNUTLS_CPUID_OVERRIDE=0x1A ${PROG}
ret=$?
if test $ret != 0; then
	echo "PCLMUL-AVX cipher tests failed"
	exit $ret
fi

echo SHANI
if (lscpu --version) >/dev/null 2>&1 && \
   lscpu 2>/dev/null | grep 'Flags:[	]*sha_ni' >/dev/null; then
	GNUTLS_CPUID_OVERRIDE=0x20 ${PROG}
	ret=$?
	if test $ret != 0; then
		echo "SHANI cipher tests failed"
		exit $ret
	fi
fi

echo padlock
GNUTLS_CPUID_OVERRIDE=0x100000 ${PROG}
ret=$?
if test $ret != 0; then
	echo "padlock cipher tests failed"
	exit $ret
fi

echo padlock PHE
GNUTLS_CPUID_OVERRIDE=0x200000 ${PROG}
ret=$?
if test $ret != 0; then
	echo "padlock PHE cipher tests failed"
	exit $ret
fi

echo padlock PHE SHA512
GNUTLS_CPUID_OVERRIDE=0x400000 ${PROG}
ret=$?
if test $ret != 0; then
	echo "padlock PHE SHA512 cipher tests failed"
	exit $ret
fi

exit 0
