#!/bin/sh
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

core_files=\
"testfile_aarch64_core \
 testfile-backtrace-demangle.core \
 testfiledwarfinlines.core \
 testfile_i686_core \
 testfile-m68k-core \
 testfile-riscv64-core \
 backtrace.aarch64.core \
 backtrace.i386.core \
 backtrace.ppc.core \
 backtrace.s390.core"

testfiles $core_files

echo "elfclassify --core"
testrun ${abs_top_builddir}/src/elfclassify --core $core_files
testrun_compare ${abs_top_builddir}/src/elfclassify --core --print $core_files <<EOF
$(echo $core_files | sed -e "s/ /\n/g")
EOF

echo "core files are not programs"
testrun ${abs_top_builddir}/src/elfclassify --not-program $core_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-program --print $core_files <<EOF
$(echo $core_files | sed -e "s/ /\n/g")
EOF

echo "core files are not shared"
testrun ${abs_top_builddir}/src/elfclassify --not-shared $core_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-shared --print $core_files <<EOF
$(echo $core_files | sed -e "s/ /\n/g")
EOF

echo "core files are not kernel-modules"
testrun ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module $core_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module --print $core_files <<EOF
$(echo $core_files | sed -e "s/ /\n/g")
EOF

echo "core files are not debug-only"
testrun ${abs_top_builddir}/src/elfclassify --not-debug-only $core_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-debug-only --print $core_files <<EOF
$(echo $core_files | sed -e "s/ /\n/g")
EOF

object_files=\
"debug-ranges-no-lowpc.o \
 testfile-annobingroup-i386.o \
 testfile-bpf-dis1.o \
 testfile-debug-rel-g.o \
 testfile-gnu-property-note.o"

testfiles $object_files

echo "elfclassify --elf-file"
testrun ${abs_top_builddir}/src/elfclassify --elf-file $object_files
testrun_compare ${abs_top_builddir}/src/elfclassify --elf-file --print $object_files <<EOF
$(echo $object_files | sed -e "s/ /\n/g")
EOF

echo "object files are not archives"
testrun ${abs_top_builddir}/src/elfclassify --not-elf-archive $object_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-elf-archive --print $object_files <<EOF
$(echo $object_files | sed -e "s/ /\n/g")
EOF

echo "object files are not core files"
testrun ${abs_top_builddir}/src/elfclassify --not-core $object_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-core --print $object_files <<EOF
$(echo $object_files | sed -e "s/ /\n/g")
EOF

echo "object files are not program files"
testrun ${abs_top_builddir}/src/elfclassify --not-program $object_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-program --print $object_files <<EOF
$(echo $object_files | sed -e "s/ /\n/g")
EOF

echo "object files are not shared files"
testrun ${abs_top_builddir}/src/elfclassify --not-shared $object_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-shared --print $object_files <<EOF
$(echo $object_files | sed -e "s/ /\n/g")
EOF

echo "object files are not kernel modules"
testrun ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module $object_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module --print $object_files <<EOF
$(echo $object_files | sed -e "s/ /\n/g")
EOF

echo "object files are not debug-only files"
testrun ${abs_top_builddir}/src/elfclassify --not-debug-only $object_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-debug-only --print $object_files <<EOF
$(echo $object_files | sed -e "s/ /\n/g")
EOF

ar_files="testarchive64.a"

testfiles $ar_files

echo "elfclassify --elf-archive"
testrun ${abs_top_builddir}/src/elfclassify --elf-archive $ar_files
testrun_compare ${abs_top_builddir}/src/elfclassify --elf-archive --print $ar_files <<EOF
$(echo $ar_files | sed -e "s/ /\n/g")
EOF

echo "archives are not elf-files"
testrun ${abs_top_builddir}/src/elfclassify --not-elf-file $ar_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-elf-file --print $ar_files <<EOF
$(echo $ar_files | sed -e "s/ /\n/g")
EOF

echo "archives are not core files"
testrun ${abs_top_builddir}/src/elfclassify --not-core $ar_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-core --print $ar_files <<EOF
$(echo $ar_files | sed -e "s/ /\n/g")
EOF

echo "archives are not program files"
testrun ${abs_top_builddir}/src/elfclassify --not-program $ar_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-program --print $ar_files <<EOF
$(echo $ar_files | sed -e "s/ /\n/g")
EOF

echo "archives are not shared files"
testrun ${abs_top_builddir}/src/elfclassify --not-shared $ar_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-shared --print $ar_files <<EOF
$(echo $ar_files | sed -e "s/ /\n/g")
EOF

lib_files=\
"testfile52-32.noshdrs.so \
 libtestfile_multi_shared.so \
 testfile52-32.prelink.so \
 testfile52-32.so
 testfile54-64.noshdrs.so \
 testfile54-64.prelink.so \
 testfile54-64.so \
 testlib_dynseg.so"

testfiles $lib_files

echo "elfclassify --shared"
testrun ${abs_top_builddir}/src/elfclassify --shared $lib_files
testrun_compare ${abs_top_builddir}/src/elfclassify --shared --print $lib_files <<EOF
$(echo $lib_files | sed -e "s/ /\n/g")
EOF

echo "shared files are loadable"
testrun ${abs_top_builddir}/src/elfclassify --loadable $lib_files
testrun_compare ${abs_top_builddir}/src/elfclassify --loadable --print $lib_files <<EOF
$(echo $lib_files | sed -e "s/ /\n/g")
EOF

echo "shared files are not executables"
testrun ${abs_top_builddir}/src/elfclassify --not-executable $lib_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-executable --print $lib_files <<EOF
$(echo $lib_files | sed -e "s/ /\n/g")
EOF

echo "shared files are not debug-only"
testrun ${abs_top_builddir}/src/elfclassify --not-debug-only $lib_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-debug-only --print $lib_files <<EOF
$(echo $lib_files | sed -e "s/ /\n/g")
EOF

echo "shared files are not kernel modules"
testrun ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module $lib_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module --print $lib_files <<EOF
$(echo $lib_files | sed -e "s/ /\n/g")
EOF

exe_files=\
"backtrace.aarch64.exec \
 backtrace.i386.exec \
 backtrace.ppc.exec \
 backtrace.s390x.exec \
 testfile70.exec \
 test-offset-loop \
 testfilebaztab \
 testfilebaztabppc64"

testfiles $exe_files

echo "elfclassify --program"
testrun ${abs_top_builddir}/src/elfclassify --program $exe_files
testrun_compare ${abs_top_builddir}/src/elfclassify --program --print $exe_files <<EOF
$(echo $exe_files | sed -e "s/ /\n/g")
EOF

echo "programs are executables (in this case)"
testrun ${abs_top_builddir}/src/elfclassify --executable $exe_files
testrun_compare ${abs_top_builddir}/src/elfclassify --executable --print $exe_files <<EOF
$(echo $exe_files | sed -e "s/ /\n/g")
EOF

echo "programs are not shared libraries (in this case)"
testrun ${abs_top_builddir}/src/elfclassify --not-shared $exe_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-shared --print $exe_files <<EOF
$(echo $exe_files | sed -e "s/ /\n/g")
EOF

echo "programs are not kernel-modules"
testrun ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module $exe_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module --print $exe_files <<EOF
$(echo $exe_files | sed -e "s/ /\n/g")
EOF

echo "programs are not debug-only"
testrun ${abs_top_builddir}/src/elfclassify --not-debug-only $exe_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-debug-only --print $exe_files <<EOF
$(echo $exe_files | sed -e "s/ /\n/g")
EOF

kmod_files=\
"hello_aarch64.ko \
 hello_csky.ko \
 hello_i386.ko \
 hello_m68k.ko \
 hello_ppc64.ko \
 hello_riscv64.ko \
 hello_s390.ko \
 hello_x86_64.ko"

testfiles $kmod_files

echo "elfclassify --linux-kernel-module"
testrun ${abs_top_builddir}/src/elfclassify --linux-kernel-module $kmod_files
testrun_compare ${abs_top_builddir}/src/elfclassify --linux-kernel-module --print $kmod_files <<EOF
$(echo $kmod_files | sed -e "s/ /\n/g")
EOF

echo "kmods are unstripped"
testrun ${abs_top_builddir}/src/elfclassify --unstripped $kmod_files
testrun_compare ${abs_top_builddir}/src/elfclassify --unstripped --print $kmod_files <<EOF
$(echo $kmod_files | sed -e "s/ /\n/g")
EOF

echo "kmods are not debug-only"
testrun ${abs_top_builddir}/src/elfclassify --not-debug-only $kmod_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-debug-only --print $kmod_files <<EOF
$(echo $kmod_files | sed -e "s/ /\n/g")
EOF

echo "kmods are not loabable (in the normal sense)"
testrun ${abs_top_builddir}/src/elfclassify --not-loadable $kmod_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-loadable --print $kmod_files <<EOF
$(echo $kmod_files | sed -e "s/ /\n/g")
EOF

echo "gnu compressed kmods are unstripped"
testrun ${abs_top_builddir}/src/elfcompress -t gnu --force $kmod_files
testrun ${abs_top_builddir}/src/elfclassify --unstripped $kmod_files
testrun_compare ${abs_top_builddir}/src/elfclassify --unstripped --print $kmod_files <<EOF
$(echo $kmod_files | sed -e "s/ /\n/g")
EOF

debug_files=\
"testfile15.debug \
 testfile35.debug \
 testfile40.debug \
 testfile48.debug \
 testfile53-32.debug \
 testfile53-64.debug \
 testfilebazdbg.debug \
 testfilebazdbgppc64.debug \
 addrx_constx-4.dwo \
 addrx_constx-5.dwo"

testfiles $debug_files

echo "elfclassify --debug-only"
testrun ${abs_top_builddir}/src/elfclassify --debug-only $debug_files
testrun_compare ${abs_top_builddir}/src/elfclassify --debug-only --print $debug_files <<EOF
$(echo $debug_files | sed -e "s/ /\n/g")
EOF

echo "debug-only files are unstripped"
testrun ${abs_top_builddir}/src/elfclassify --unstripped $debug_files
testrun_compare ${abs_top_builddir}/src/elfclassify --unstripped --print $debug_files <<EOF
$(echo $debug_files | sed -e "s/ /\n/g")
EOF

echo "debug-only files are not programs"
testrun ${abs_top_builddir}/src/elfclassify --not-program $debug_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-program --print $debug_files <<EOF
$(echo $debug_files | sed -e "s/ /\n/g")
EOF

echo "debug-only files are not shared"
testrun ${abs_top_builddir}/src/elfclassify --not-shared $debug_files
testrun_compare ${abs_top_builddir}/src/elfclassify --not-shared --print $debug_files <<EOF
$(echo $debug_files | sed -e "s/ /\n/g")
EOF

echo "compress the debug sections and try again"
testrun ${abs_top_builddir}/src/elfcompress -t gnu --force $debug_files

echo "again unstripped"
testrun ${abs_top_builddir}/src/elfclassify --unstripped $debug_files
testrun_compare ${abs_top_builddir}/src/elfclassify --unstripped --print $debug_files <<EOF
$(echo $debug_files | sed -e "s/ /\n/g")
EOF

echo "again debug-only"
testrun ${abs_top_builddir}/src/elfclassify --debug-only $debug_files
testrun_compare ${abs_top_builddir}/src/elfclassify --debug-only --print $debug_files <<EOF
$(echo $debug_files | sed -e "s/ /\n/g")
EOF
