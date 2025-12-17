#!/bin/sh

# Copyright (C) 2021 Daiki Ueno
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
: ${srcdir=.}

: ${DUMPCFG=$top_builddir/src/dumpcfg${EXEEXT}}

if ! test -x "${DUMPCFG}"; then
	exit 77
fi

if test -n "$VALGRIND"; then
	VALGRIND="${LIBTOOL:-libtool} --mode=execute ${VALGRIND} --error-exitcode=1"
fi

: ${DIFF=diff}

. "$srcdir/scripts/common.sh"
testdir=`create_testdir cfg`


TEMPLATES="
arb-extensions.tmpl
crit-extensions.tmpl
inhibit-anypolicy.tmpl
template-crq.tmpl
template-date.tmpl
template-dates-after2038.tmpl
template-dn-err.tmpl
template-dn.tmpl
template-generalized.tmpl
template-krb5name.tmpl
template-long-dns.tmpl
template-long-serial.tmpl
template-nc.tmpl
template-no-ca-explicit.tmpl
template-no-ca-honor.tmpl
template-no-ca.tmpl
template-othername-xmpp.tmpl
template-othername.tmpl
template-overflow.tmpl
template-overflow2.tmpl
template-test.tmpl
template-tlsfeature-crq.tmpl
template-tlsfeature.tmpl
template-unique.tmpl
template-utf8.tmpl
simple-policy.tmpl
"

for template in $TEMPLATES; do
    "$DUMPCFG" "$srcdir/cert-tests/templates/$template" > "$testdir/$template.out"
    "$DIFF" "$srcdir/fixtures/templates/$template.exp" "$testdir/$template.out" || exit 1
done

rm -rf "$testdir"
