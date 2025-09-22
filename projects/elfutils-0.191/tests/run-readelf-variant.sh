#! /bin/sh
# Copyright (C) 2017 Red Hat, Inc.
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

# Tests exprloc for an Ada record variants byte_size.

# = pck.ads
#
# with System;
#
# package Pck is
#
#    type One_To_Five is range 1 .. 5;
#
#    type Rec (Discr : One_To_Five) is
#    record
#       case Discr is
#          when 1 => Field1 : Integer;
#          when 4 => null;
#          when 3 => Field3 : Boolean;
#          when 5 => null;
#          when others => null;
#       end case;
#    end record;
#
#    procedure Do_Nothing (A : System.Address);
#
# end Pck;

# = pck.adb
#
# package body Pck is
#
#    procedure Do_Nothing (A : System.Address) is
#    begin
#       null;
#    end Do_Nothing;
#
# end Pck;

# = foo.adb
#
# with Pck; use Pck;
#
# procedure Foo is
#
#    R : Rec (1);
#
# begin
#    Do_Nothing (R'Address);
# end Foo;

# gnatmake -g -fgnat-encodings=minimal foo.adb -cargs

testfiles testfile-ada-variant

tempfiles testfile.temp testfile2.temp

testrun ${abs_top_builddir}/src/readelf --debug-dump=info \
        testfile-ada-variant > testfile.temp

grep -A6 byte_size testfile.temp | grep -A6 exprloc > testfile2.temp

diff -u testfile2.temp - <<EOF
             byte_size            (exprloc) 
              [ 0] push_object_address
              [ 1] deref_size 1
              [ 3] call4 [    95]
              [ 8] plus_uconst 7
              [10] const1s -4
              [12] and
EOF

exit 0
