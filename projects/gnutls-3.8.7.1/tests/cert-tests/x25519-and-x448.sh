#!/bin/sh

# Copyright (C) 2021 Free Software Foundation, Inc.
#
# Author: Daniel Kahn Gillmor
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
TMPFILE=crfg-kx.$$.tmp
TMPCA=eddsa-ca.$$.tmp
TMPCAKEY=eddsa-ca-key.$$.tmp
TMPSUBCA=eddsa-subca.$$.tmp
TMPSUBCAKEY=eddsa-subca-key.$$.tmp
TMPKEY=kx-key.$$.tmp
TMPTEMPL=template.$$.tmp
TMPUSER=user.$$.tmp
VERIFYOUT=verify.$$.tmp

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

for curve in 25519 448; do
    echo ca > $TMPTEMPL
    echo "cn = Ed$curve CA" >> $TMPTEMPL

    "${CERTTOOL}" --generate-privkey --key-type=ed$curve > $TMPCAKEY 2>/dev/null

    "${CERTTOOL}" -d 2 --generate-self-signed --template $TMPTEMPL \
	          --load-privkey $TMPCAKEY \
	          --outfile $TMPCA >$TMPFILE 2>&1

    if [ $? != 0 ]; then
	cat $TMPFILE
	exit 1
    fi

    echo ca > $TMPTEMPL
    echo "cn = Ed$curve Mid CA" >> $TMPTEMPL

    "${CERTTOOL}" --generate-privkey --key-type=ed$curve > $TMPSUBCAKEY 2>/dev/null

    "${CERTTOOL}" -d 2 --generate-certificate --template $TMPTEMPL \
	          --load-ca-privkey $TMPCAKEY \
	          --load-ca-certificate $TMPCA \
	          --load-privkey $TMPSUBCAKEY \
	          --outfile $TMPSUBCA >$TMPFILE 2>&1

    if [ $? != 0 ]; then
	cat $TMPFILE
	exit 1
    fi

    echo "cn = End-user" > $TMPTEMPL
    echo email_protection_key >> $TMPTEMPL
    echo encryption_key >> $TMPTEMPL

    "${CERTTOOL}" --generate-privkey --key-type=x$curve > $TMPKEY 2>/dev/null

    "${CERTTOOL}" -d 2 --generate-certificate --template $TMPTEMPL \
	          --load-ca-privkey $TMPSUBCAKEY \
	          --load-ca-certificate $TMPSUBCA \
	          --load-privkey $TMPKEY \
	          --outfile $TMPUSER >$TMPFILE 2>&1

    if [ $? != 0 ]; then
	cat $TMPFILE
	exit 1
    fi

    cat $TMPUSER $TMPSUBCA $TMPCA > $TMPFILE
    "${CERTTOOL}" --verify-chain <$TMPFILE > $VERIFYOUT

    if [ $? != 0 ]; then
	cat $VERIFYOUT
	exit 1
    fi

    rm -f $VERIFYOUT $TMPUSER $TMPCA $TMPSUBCA $TMPTEMPL $TMPFILE
    rm -f $TMPSUBCAKEY $TMPCAKEY $TMPKEY
done

exit 0
