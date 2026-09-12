# plural-depth.awk - Generate .po file with deeply nested plural expression.
# Copyright (C) 2026 Free Software Foundation, Inc.
#
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

# Generate a .po file whose Plural-Forms header contains a plural
# expression nested DEPTH levels deep.  Each level wraps as !(1-(...)),
# producing an expression that is accepted by the parser (YYMAXDEPTH=10000)
# but exceeds EVAL_MAXDEPTH=100 at runtime.
#
# Usage: awk -v DEPTH=5000 -f plural-depth.awk > plural-depth.po

BEGIN {
    if (DEPTH == 0)
	DEPTH = 5000

    expr = ""
    for (i = 0; i < DEPTH; i++)
	expr = expr "!(1-"
    expr = expr "(n!=1)"
    for (i = 0; i < DEPTH; i++)
	expr = expr ")"

    print "msgid \"\""
    print "msgstr \"\""
    print "\"Project-Id-Version: test\\n\""
    print "\"PO-Revision-Date: 2026-01-01 00:00+0000\\n\""
    print "\"Last-Translator: \\n\""
    print "\"Language-Team: \\n\""
    print "\"Language: ll\\n\""
    print "\"MIME-Version: 1.0\\n\""
    print "\"Content-Type: text/plain; charset=ASCII\\n\""
    print "\"Content-Transfer-Encoding: 8bit\\n\""
    print "\"Plural-Forms: nplurals=2; plural=" expr ";\\n\""
    print ""
    print "msgid \"X\""
    print "msgid_plural \"Y\""
    print "msgstr[0] \"x\""
    print "msgstr[1] \"y\""
}
