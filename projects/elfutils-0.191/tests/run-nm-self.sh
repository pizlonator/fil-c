#! /bin/sh
# Copyright (C) 2012, 2014 Red Hat, Inc.
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

# Test different command line combinations on the nm binary itself.
# Test on nm ET_REL, ET_EXEC and ET_DYN files.
ET_REL=${abs_top_builddir}/src/nm.o
ET_EXEC=${abs_top_builddir}/src/nm
ET_DYN=${abs_top_builddir}/libelf/libelf.so
for what_arg in --debug-syms --defined-only --dynamic --extern-only; do
  for format_arg in --format=bsd --format=sysv --format=posix; do
    for out_arg in --numeric-sort --no-sort --reverse-sort; do
      for self_file in $ET_REL $ET_EXEC $ET_DYN; do
	# --dynamic doesn't make sense for ET_REL.
	if ! test "$what_arg" = "--dynamic" -a "$self_file" = "$ET_REL"; then
	  testrun ${abs_top_builddir}/src/nm $what_arg $format_arg $out_arg $self_file > /dev/null
	fi
      done
    done
  done
done
