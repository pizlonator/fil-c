#!/bin/sh
# Test plural expression evaluation hardening.
# Copyright (C) 2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

set -e

common_objpfx=$1
test_program_prefix=$2
objpfx=$3

# Create domain directories.
mkdir -p ${objpfx}domaindir/ll/LC_MESSAGES

# Test 1: Deeply nested plural expression (stack overflow test).
# This expression has 5000 levels of nesting, well above EVAL_MAXDEPTH=100
# but below YYMAXDEPTH=10000 so the parser accepts it.
LC_ALL=C awk -v DEPTH=5000 -f plural-depth.awk > ${objpfx}plural-depth.po

msgfmt -o ${objpfx}domaindir/ll/LC_MESSAGES/plural-depth.mo \
       ${objpfx}plural-depth.po || exit 1

# Test 2: Division by zero in plural expression (SIGFPE test).
# The expression 1/(n!=1729) triggers division by zero for n=1729.
# msgfmt -c only checks 0 <= n <= 1000, so this passes validation.
cat > ${objpfx}plural-divzero.po <<EOF
msgid ""
msgstr ""
"Project-Id-Version: test\n"
"PO-Revision-Date: 2026-01-01 00:00+0000\n"
"Last-Translator: \n"
"Language-Team: \n"
"Language: ll\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=ASCII\n"
"Content-Transfer-Encoding: 8bit\n"
"Plural-Forms: nplurals=3; plural=(n!=1)+1/(n!=1729);\n"

msgid "X"
msgid_plural "Y"
msgstr[0] "x"
msgstr[1] "y"
msgstr[2] "z"
EOF

msgfmt -o ${objpfx}domaindir/ll/LC_MESSAGES/plural-divzero.mo \
       ${objpfx}plural-divzero.po || exit 1

# Run the test.
${test_program_prefix} \
${objpfx}tst-plural-eval ${objpfx}domaindir

exit $?
