#!/bin/sh
# Copyright (C) 2000-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#

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

# Checks that the iconv() implementation (in both directions) for a
# stateless encoding agrees with the charmap table.

common_objpfx=$1
objpfx=$2
test_program_prefix=$3
charset=$4
charmap=$5

# sort is used on the build system.
LC_ALL=C
export LC_ALL

set -e

# Get the charmap.
./tst-table-charmap.sh \
  < ../localedata/charmaps/${charmap:-$charset} \
  > ${objpfx}tst-${charset}.charmap.table
# When the charset is GB18030, truncate this table because for this encoding,
# the charmap contains ranges (<Unnnn>..<Ummmm> notation), which the
# tst-table-charmap.sh script does not grok.
if test ${charset} = GB18030; then
  grep '0x....$' < ${objpfx}tst-${charset}.charmap.table \
    > ${objpfx}tst-${charset}.truncated.table
  mv ${objpfx}tst-${charset}.truncated.table ${objpfx}tst-${charset}.charmap.table
fi

# Precomputed expected differences between the charmap and iconv forward.
precomposed=${charset}.precomposed

# Precompute expected differences between the charmap and iconv backward.
if test ${charset} = EUC-TW; then
  irreversible=${objpfx}tst-${charset}.irreversible
  (grep '^0x8EA1' ${objpfx}tst-${charset}.charmap.table
   cat ${charset}.irreversible
  ) > ${irreversible}
else
  irreversible=${charset}.irreversible
fi

# iconv in one direction.
${test_program_prefix} \
${objpfx}tst-table-from ${charset} \
  > ${objpfx}tst-${charset}.table

# iconv in the other direction.
${test_program_prefix} \
${objpfx}tst-table-to ${charset} | sort \
  > ${objpfx}tst-${charset}.inverse.table

# Difference between the charmap and iconv backward.
diff ${objpfx}tst-${charset}.charmap.table ${objpfx}tst-${charset}.inverse.table | \
  grep '^[<>]' | sed -e 's,^. ,,' > ${objpfx}tst-${charset}.irreversible.table

# Check 1: charmap and iconv forward should be identical, except for
# precomposed characters.
{ if test -f ${precomposed}; then
    cat ${objpfx}tst-${charset}.table ${precomposed} | sort | uniq -u
  else
    cat ${objpfx}tst-${charset}.table
  fi
} | { if test ${charset} = GB18030; then grep '0x....$'; else cat; fi; } \
  > ${objpfx}tst-${charset}.tmp1.table
cmp -s ${objpfx}tst-${charset}.charmap.table ${objpfx}tst-${charset}.tmp1.table ||
  exit 1

# Check 2: the difference between the charmap and iconv backward.
{ if test -f ${irreversible}; then
    cat ${objpfx}tst-${charset}.charmap.table ${irreversible} | sort | uniq -u
  else
    cat ${objpfx}tst-${charset}.charmap.table
  fi
} | { if test ${charset} = GB18030; then grep '0x....$'; else cat; fi; } \
  > ${objpfx}tst-${charset}.tmp2c.table
cat ${objpfx}tst-${charset}.inverse.table \
  | { if test ${charset} = GB18030; then grep '0x....$'; else cat; fi; } \
  > ${objpfx}tst-${charset}.tmp2i.table
cmp -s ${objpfx}tst-${charset}.tmp2c.table ${objpfx}tst-${charset}.tmp2i.table ||
  exit 1

# Check 3: the difference between iconv forward and iconv backward. This is
# necessary only for GB18030, because ${objpfx}tst-${charset}.charmap.table
# is truncated for this encoding (see above).
if test ${charset} = GB18030; then
  { if test -f ${irreversible}; then
      cat ${objpfx}tst-${charset}.table ${irreversible} | sort | uniq -u
    else
      cat ${objpfx}tst-${charset}.table
    fi
  } > ${objpfx}tst-${charset}.tmp3.table
  cmp -s ${objpfx}tst-${charset}.tmp3.table ${objpfx}tst-${charset}.inverse.table ||
    exit 1
fi

exit 0
