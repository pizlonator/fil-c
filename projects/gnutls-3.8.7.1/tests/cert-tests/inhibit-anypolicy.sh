#!/bin/sh

# Copyright (C) 2017 Red Hat, Inc.
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
: ${DIFF=diff}

if ! test -x "${CERTTOOL}"; then
	exit 77
fi

export TZ="UTC"
TMPFILE=tmp-inhibit.pem.$$.tmp
TEMPLFILE=template.inhibit.$$.tmp
CAFILE=inhibit-ca.$$.tmp
SUBCAFILE=inhibit-subca.$$.tmp

. ${srcdir}/../scripts/common.sh

"${CERTTOOL}" --attime "2017-04-22" --generate-self-signed \
	--load-privkey "${srcdir}/data/key-ca.pem" \
	--template "${srcdir}/templates/inhibit-anypolicy.tmpl" \
	--outfile ${CAFILE} 2>/dev/null

${DIFF} "${srcdir}/data/inhibit-anypolicy.pem" ${CAFILE}
rc=$?

# We're done.
if test "${rc}" != "0"; then
	echo "CA generation failed ${CAFILE}"
	exit ${rc}
fi

# generate leaf
echo ca > $TEMPLFILE
echo "cn = sub-CA" >> $TEMPLFILE

"${CERTTOOL}" --attime "2017-04-23" -d 2 --generate-certificate --template $TEMPLFILE \
	--load-ca-privkey "${srcdir}/data/key-ca.pem" \
	--load-ca-certificate $CAFILE \
	--load-privkey "${srcdir}/data/key-subca.pem" \
	--outfile $SUBCAFILE

if [ $? != 0 ]; then
	cat $TMPFILE
	exit 1
fi

cat $SUBCAFILE $CAFILE > ${TMPFILE}

# we do not support the inhibit any policy extension for verification
"${CERTTOOL}"  --attime "2017-04-25" --verify-chain --infile ${TMPFILE}
rc=$?
if test "$rc" != "0"; then
	echo "Verification failed unexpectedly ($rc)"
	exit 1
fi

rm -f ${TMPFILE}
rm -f ${TEMPLFILE}
rm -f ${CAFILE}
rm -f ${SUBCAFILE}

exit 0
