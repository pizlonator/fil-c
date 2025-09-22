#! /bin/sh
# Copyright (C) 2019 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/test-subr.sh

test_reverse_self ()
{
  in_file="$1"
  base_name="$(basename ${in_file})"
  out_file="${base_name}.rev"
  out_file_mmap="${out_file}.mmap"

  tempfiles ${out_file} ${out_file_mmap}

  # Reverse the offsets (the files should still be the same otherwise)
  testrun ${abs_builddir}/elfcopy --reverse-offs ${in_file} ${out_file}
  testrun ${abs_top_builddir}/src/elfcmp ${in_file} ${out_file}
  testrun ${abs_top_builddir}/src/elflint --gnu ${out_file}
  # An in-place nop will likely revert them back
  testrun ${abs_builddir}/elfrdwrnop ${out_file}
  testrun ${abs_top_builddir}/src/elfcmp ${in_file} ${out_file}
  testrun ${abs_top_builddir}/src/elflint --gnu ${out_file}
}

# Only really makes sense for ET_REL files, but try all, just to check
# it also works if we keep the order for the allocated sections.
for file in $self_test_files; do
  test_reverse_self $file
done

exit 0
