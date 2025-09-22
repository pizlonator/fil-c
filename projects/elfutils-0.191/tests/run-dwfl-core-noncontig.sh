#! /bin/sh
# Copyright (C) 2023 Red Hat, Inc.
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

# Test whether libdwfl can handle corefiles containing non-contiguous
# segments where multiple modules are contained within the address
# space of some other module.

# testcore-noncontig was generated from the following program with
# systemd-coredump on RHEL 7.9 Workstation, kernel
# 3.10.0-1160.105.1.el7.x86_64. liblgpllibs.so was packaged with
# firefox-115.4.0-1.el7_9.x86_64.rpm.

# #include <unistd.h>
# #include <dlfcn.h>
#
# int main () {
#   dlopen ("/usr/lib64/firefox/liblgpllibs.so", RTLD_GLOBAL | RTLD_NOW);
#   sleep (60);
#   return 0;
# }
#
# gcc -ldl -o test test.c

tempfiles out
testfiles testcore-noncontig

testrun ${abs_builddir}/dwfl-core-noncontig testcore-noncontig

# Remove parts of the output that could change depending on which
# libraries are locally installed.
testrun ${abs_top_builddir}/src/unstrip -n --core testcore-noncontig \
  | sed 's/+/ /g' | cut -d " " -f1,3 | sort > out

testrun_compare cat out <<\EOF
0x400000 3a1748a544b40a38b3be3d2d13ffa34a2a5a71c0@0x400284
0x7f14e357e000 edf51350c7f71496149d064aa8b1441f786df88a@0x7f14e357e1d8
0x7f14e3794000 7615604eaf4a068dfae5085444d15c0dee93dfbd@0x7f14e37941d8
0x7f14e3a96000 09cfb171310110bc7ea9f4476c9fa044d85baff4@0x7f14e3a96210
0x7f14e3d9e000 e10cc8f2b932fc3daeda22f8dac5ebb969524e5b@0x7f14e3d9e248
0x7f14e3fba000 fc4fa58e47a5acc137eadb7689bce4357c557a96@0x7f14e3fba280
0x7f14e4388000 7f2e9cb0769d7e57bd669b485a74b537b63a57c4@0x7f14e43881d8
0x7f14e458c000 62c449974331341bb08dcce3859560a22af1e172@0x7f14e458c1d8
0x7f14e4795000 175efdcef445455872a86a6fbee7567ca16a513e@0x7f14e4795248
0x7ffcfe59f000 80d79b32785868a2dc10047b39a80d1daec8923d@0x7ffcfe59f328
EOF

exit 0
