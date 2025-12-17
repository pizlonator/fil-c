#!/bin/sh

# Copyright (C) 2017 Nikos Mavrogiannopoulos
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

: ${srcdir=.}
: ${top_builddir=..}
: ${PKG_CONFIG=pkg-config}
: ${CC=cc}
unset RETCODE
TMPFILE=c.$$.tmp.c
TMPFILE_O=c.$$.tmp.o

echo "$CFLAGS"|grep sanitize && exit 77

${PKG_CONFIG} --version >/dev/null || exit 77

${PKG_CONFIG} --libs nettle
if test $? != 0;then
	echo "Nettle was not found in pkg-config"
	exit 77
fi

for lib in libidn2 p11-kit-1
do
	OTHER=$(${PKG_CONFIG} --libs --static $lib)
	if test -n "${OTHER}" && test "${OTHER#*-R}" != "$OTHER";then
		echo "Found invalid string in $lib flags: ${OTHER}"
		exit 77
	fi
done

if ! test -r ${top_builddir}/lib/gnutls.pc ;then
	echo "gnutls.pc not present at ${top_builddir}/lib"
	exit 1
fi

PKG_CONFIG_PATH=${top_builddir}/lib:$PKG_CONFIG_PATH
export PKG_CONFIG_PATH

set -e

cat >$TMPFILE <<__EOF__
#include <gnutls/gnutls.h>

int main(void)
{
gnutls_global_init();
}
__EOF__

COMMON="-I${top_builddir}/lib/includes -L${top_builddir}/lib/.libs -I${srcdir}/../lib/includes"
echo "Trying dynamic linking with:"
echo "  * flags: $(${PKG_CONFIG} --libs gnutls)"
echo "  * common: ${COMMON}"
echo "  * cflags: ${CFLAGS}"
echo "  * ldflags: ${LDFLAGS}"
echo cc ${TMPFILE} -o ${TMPFILE_O} $(${PKG_CONFIG} --libs gnutls) $(${PKG_CONFIG} --cflags gnutls) ${COMMON} ${CFLAGS} ${LDFLAGS}
${CC} ${TMPFILE} -o ${TMPFILE_O} $(${PKG_CONFIG} --libs gnutls) $(${PKG_CONFIG} --cflags gnutls) ${COMMON} ${CFLAGS} ${LDFLAGS}

echo ""
echo "Trying static linking with $(${PKG_CONFIG} --libs --static gnutls)"
echo cc ${TMPFILE} -o ${TMPFILE_O} $(${PKG_CONFIG} --static --libs gnutls) $(${PKG_CONFIG} --cflags gnutls) ${COMMON} ${CFLAGS} ${LDFLAGS}
${CC} ${TMPFILE} -o ${TMPFILE_O} $(${PKG_CONFIG} --static --libs gnutls) $(${PKG_CONFIG} --cflags gnutls) ${COMMON} ${CFLAGS} ${LDFLAGS}

rm -f ${TMPFILE} ${TMPFILE_O}

exit 0
