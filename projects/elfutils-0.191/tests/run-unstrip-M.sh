#! /bin/sh
# Copyright (C) 2013 Red Hat, Inc.
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

# Linux /proc/PID/maps file with some non-file entries (and fake exe/lib names).
tempfiles testmaps
cat > testmaps <<EOF
00400000-024aa000 r-xp 00000000 08:02 159659                             /opt/TestBins/bin/arwijn
026aa000-026b2000 rwxp 020aa000 08:02 159659                             /opt/TestBins/bin/arwijn
026b2000-026bf000 rwxp 00000000 00:00 0
0335a000-03e6f000 rwxp 00000000 00:00 0                                  [heap]
2b7b38282000-2b7b38302000 rwxs 00000000 00:06 493872                     socket:[493872]
2b7b38302000-2b7b38312000 rwxs 00000000 00:06 493872                     socket:[493872]
2b7b38312000-2b7b38b12000 r-xs 00000000 00:06 493872                     socket:[493872]
2b7b38b12000-2b7b38b22000 rwxs 00000000 00:06 493872                     socket:[493872]
2b7b38b22000-2b7b39322000 rwxs 00000000 00:06 493872                     socket:[493872]
2b7b4439f000-2b7b45ea1000 rwxp 00000000 00:00 0
7f31e7d9f000-7f31e7f29000 r-xp 00000000 fd:00 917531                     /lib64/libc-1.13.so
7f31e7f29000-7f31e8128000 ---p 0018a000 fd:00 917531                     /lib64/libc-1.13.so
7f31e8128000-7f31e812c000 r--p 00189000 fd:00 917531                     /lib64/libc-1.13.so
7f31e812c000-7f31e812d000 rw-p 0018d000 fd:00 917531                     /lib64/libc-1.13.so
7f31e812d000-7f31e8132000 rw-p 00000000 00:00 0 
7f31ea3f9000-7f31ea3fc000 rw-s 00000000 00:09 3744                       anon_inode:kvm-vcpu
7f31ea3fc000-7f31ea3ff000 rw-s 00000000 00:09 3744                       anon_inode:kvm-vcpu
7f31ea400000-7f31ea402000 rw-p 00000000 00:00 0 
7fff26cf7000-7fff26d0c000 rwxp 00000000 00:00 0                          [stack]
7fff26dff000-7fff26e00000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
EOF

testrun_compare ${abs_top_builddir}/src/unstrip -n -M testmaps <<\EOF
0x400000+0x22b2000 - - - /opt/TestBins/bin/arwijn
0x7f31e7d9f000+0x38e000 - - - /lib64/libc-1.13.so
EOF

exit 0
