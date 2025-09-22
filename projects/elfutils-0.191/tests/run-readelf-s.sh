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

# Tests readelf -s and readelf --elf-section -s
# See also run-dwflsyms.sh
#
# - bar.c
#
# static int b1 = 1;
# int b2 = 1;
#
# static int
# foo (int a)
# {
#   return a + b2;
# }
#
# int bar (int b)
# {
#   return b - foo (b - b1);
# }
#
# - foo.c
#
# extern int bar (int b);
# extern int b2;
#
# int
# main (int argc, char ** argv)
# {
#   return bar (argc + b2);
# }
#
# gcc -pie -g -c foo.c
# gcc -pie -g -c bar.c
# gcc -pie -g -o baz foo.o bar.o
#
# - testfilebaztab (dynsym + symtab)
# cp baz testfilebaztab
#
# - testfilebazdbg (dynsym + .debug file)
# eu-strip --remove-comment -f testfilebazdbg.debug baz
# cp baz testfilebazdbg
#
#-  testfilebazdyn (dynsym only)
# objcopy --remove-section=.gnu_debuglink baz testfilebazdyn
#
# - testfilebazmdb (dynsym + gnu_debugdata + .debug)
#   This is how rpmbuild does it:
# nm -D baz --format=posix --defined-only | awk '{ print $1 }' | sort > dynsyms
# nm baz.debug --format=posix --defined-only | awk '{ if ($2 == "T" || $2 == "t") print $1 }' | sort > funcsyms
# comm -13 dynsyms funcsyms > keep_symbols
# objcopy -S --remove-section .gdb_index --remove-section .comment --keep-symbols=keep_symbols baz.debug mini_debuginfo
# rm -f mini_debuginfo.xz
# xz mini_debuginfo
# objcopy --add-section .gnu_debugdata=mini_debuginfo.xz baz
# cp baz testfilebazmdb
#
# - testfilebazmin (dynsym + gnu_debugdata)
# objcopy --remove-section=.gnu_debuglink baz testfilebazmin
#
#
# Special auxiliary only, can happen with static binaries.
# - start.c
#
# extern int main (int argc, char ** argv);
# void _start (void) { for (;;) main (1, 0); }
#
# gcc -g -c start.c
# gcc -static -nostdlib -o bas foo.o bar.o start.o
#
# eu-strip --remove-comment -f bas.debug bas
# nm bas.debug --format=posix --defined-only | awk '{ if ($2 == "T" || $2 == "t") print $1 }' | sort > funcsyms
# objcopy -S --remove-section .gdb_index --remove-section .comment --keep-symbols=funcsyms bas.debug mini_debuginfo
# rm -f mini_debuginfo.xz
# xz mini_debuginfo
# objcopy --add-section .gnu_debugdata=mini_debuginfo.xz bas
# rm bas.debug
# mv bas testfilebasmin
#
#
# Make sure that find_aux_sym doesn't corrupt relocations, avoiding a kernel
# heuristic that forces ET_EXEC->ET_DYN.  NB: ld.gold doesn't seem to produce
# the mismatched load addrs between the main file and the mini_debuginfo, so
# this is forcing ld.bfd.
#
# gcc -g -o bax foo.c bar.c -fuse-ld=bfd
# eu-strip --remove-comment -f bax.debug bax
# nm -D bax --format=posix --defined-only | awk '{ print $1 }' | sort > dynsyms
# nm bax.debug --format=posix --defined-only | awk '{ if ($2 == "T" || $2 == "t") print $1 }' | sort > funcsyms
# comm -13 dynsyms funcsyms > keep_symbols
# objcopy -S --remove-section .gdb_index --remove-section .comment --keep-symbols=keep_symbols bax.debug mini_debuginfo
# rm -f mini_debuginfo.xz
# xz mini_debuginfo
# objcopy --add-section .gnu_debugdata=mini_debuginfo.xz bax
# objcopy --remove-section=.gnu_debuglink bax testfilebaxmin


testfiles testfilebaztab
testfiles testfilebazdbg testfilebazdbg.debug
testfiles testfilebazdyn
testfiles testfilebazmdb
testfiles testfilebazmin
testfiles testfilebasmin
testfiles testfilebaxmin

tempfiles testfile.dynsym.in testfile.symtab.in testfile.minsym.in

cat > testfile.dynsym.in <<\EOF

Symbol table [ 5] '.dynsym' contains 14 entries:
 2 local symbols  String table: [ 6] '.dynstr'
  Num:            Value   Size Type    Bind   Vis          Ndx Name
    0: 0000000000000000      0 NOTYPE  LOCAL  DEFAULT    UNDEF 
    1: 0000000000000238      0 SECTION LOCAL  DEFAULT        1 
    2: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF _ITM_deregisterTMCloneTable
    3: 0000000000000000      0 FUNC    GLOBAL DEFAULT    UNDEF __libc_start_main@GLIBC_2.2.5 (2)
    4: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF __gmon_start__
    5: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF _Jv_RegisterClasses
    6: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF _ITM_registerTMCloneTable
    7: 0000000000000000      0 FUNC    WEAK   DEFAULT    UNDEF __cxa_finalize@GLIBC_2.2.5 (2)
    8: 000000000020103c      0 NOTYPE  GLOBAL DEFAULT       25 _edata
    9: 0000000000201040      0 NOTYPE  GLOBAL DEFAULT       26 _end
   10: 0000000000000860    137 FUNC    GLOBAL DEFAULT       13 __libc_csu_init
   11: 000000000020103c      0 NOTYPE  GLOBAL DEFAULT       26 __bss_start
   12: 00000000000007f0     35 FUNC    GLOBAL DEFAULT       13 main
   13: 00000000000008f0      2 FUNC    GLOBAL DEFAULT       13 __libc_csu_fini
EOF

cat > testfile.symtab.in <<\EOF

Symbol table [34] '.symtab' contains 76 entries:
 54 local symbols  String table: [35] '.strtab'
  Num:            Value   Size Type    Bind   Vis          Ndx Name
    0: 0000000000000000      0 NOTYPE  LOCAL  DEFAULT    UNDEF 
    1: 0000000000000238      0 SECTION LOCAL  DEFAULT        1 
    2: 0000000000000254      0 SECTION LOCAL  DEFAULT        2 
    3: 0000000000000274      0 SECTION LOCAL  DEFAULT        3 
    4: 0000000000000298      0 SECTION LOCAL  DEFAULT        4 
    5: 00000000000002d8      0 SECTION LOCAL  DEFAULT        5 
    6: 0000000000000428      0 SECTION LOCAL  DEFAULT        6 
    7: 00000000000004f2      0 SECTION LOCAL  DEFAULT        7 
    8: 0000000000000510      0 SECTION LOCAL  DEFAULT        8 
    9: 0000000000000530      0 SECTION LOCAL  DEFAULT        9 
   10: 0000000000000638      0 SECTION LOCAL  DEFAULT       10 
   11: 0000000000000680      0 SECTION LOCAL  DEFAULT       11 
   12: 00000000000006a0      0 SECTION LOCAL  DEFAULT       12 
   13: 00000000000006e0      0 SECTION LOCAL  DEFAULT       13 
   14: 00000000000008f4      0 SECTION LOCAL  DEFAULT       14 
   15: 0000000000000900      0 SECTION LOCAL  DEFAULT       15 
   16: 0000000000000904      0 SECTION LOCAL  DEFAULT       16 
   17: 0000000000000948      0 SECTION LOCAL  DEFAULT       17 
   18: 0000000000200dd0      0 SECTION LOCAL  DEFAULT       18 
   19: 0000000000200dd8      0 SECTION LOCAL  DEFAULT       19 
   20: 0000000000200de0      0 SECTION LOCAL  DEFAULT       20 
   21: 0000000000200de8      0 SECTION LOCAL  DEFAULT       21 
   22: 0000000000200df0      0 SECTION LOCAL  DEFAULT       22 
   23: 0000000000200fc0      0 SECTION LOCAL  DEFAULT       23 
   24: 0000000000201000      0 SECTION LOCAL  DEFAULT       24 
   25: 0000000000201030      0 SECTION LOCAL  DEFAULT       25 
   26: 000000000020103c      0 SECTION LOCAL  DEFAULT       26 
   27: 0000000000000000      0 SECTION LOCAL  DEFAULT       27 
   28: 0000000000000000      0 SECTION LOCAL  DEFAULT       28 
   29: 0000000000000000      0 SECTION LOCAL  DEFAULT       29 
   30: 0000000000000000      0 SECTION LOCAL  DEFAULT       30 
   31: 0000000000000000      0 SECTION LOCAL  DEFAULT       31 
   32: 0000000000000000      0 SECTION LOCAL  DEFAULT       32 
   33: 0000000000000000      0 FILE    LOCAL  DEFAULT      ABS crtstuff.c
   34: 0000000000200de0      0 OBJECT  LOCAL  DEFAULT       20 __JCR_LIST__
   35: 0000000000000710      0 FUNC    LOCAL  DEFAULT       13 deregister_tm_clones
   36: 0000000000000740      0 FUNC    LOCAL  DEFAULT       13 register_tm_clones
   37: 0000000000000780      0 FUNC    LOCAL  DEFAULT       13 __do_global_dtors_aux
   38: 000000000020103c      1 OBJECT  LOCAL  DEFAULT       26 completed.6137
   39: 0000000000200dd8      0 OBJECT  LOCAL  DEFAULT       19 __do_global_dtors_aux_fini_array_entry
   40: 00000000000007c0      0 FUNC    LOCAL  DEFAULT       13 frame_dummy
   41: 0000000000200dd0      0 OBJECT  LOCAL  DEFAULT       18 __frame_dummy_init_array_entry
   42: 0000000000000000      0 FILE    LOCAL  DEFAULT      ABS foo.c
   43: 0000000000000000      0 FILE    LOCAL  DEFAULT      ABS bar.c
   44: 0000000000201034      4 OBJECT  LOCAL  DEFAULT       25 b1
   45: 0000000000000814     20 FUNC    LOCAL  DEFAULT       13 foo
   46: 0000000000000000      0 FILE    LOCAL  DEFAULT      ABS crtstuff.c
   47: 0000000000000a58      0 OBJECT  LOCAL  DEFAULT       17 __FRAME_END__
   48: 0000000000200de0      0 OBJECT  LOCAL  DEFAULT       20 __JCR_END__
   49: 0000000000000000      0 FILE    LOCAL  DEFAULT      ABS 
   50: 0000000000200dd8      0 NOTYPE  LOCAL  DEFAULT       18 __init_array_end
   51: 0000000000200df0      0 OBJECT  LOCAL  DEFAULT       22 _DYNAMIC
   52: 0000000000200dd0      0 NOTYPE  LOCAL  DEFAULT       18 __init_array_start
   53: 0000000000201000      0 OBJECT  LOCAL  DEFAULT       24 _GLOBAL_OFFSET_TABLE_
   54: 00000000000008f0      2 FUNC    GLOBAL DEFAULT       13 __libc_csu_fini
   55: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF _ITM_deregisterTMCloneTable
   56: 0000000000201030      0 NOTYPE  WEAK   DEFAULT       25 data_start
   57: 000000000020103c      0 NOTYPE  GLOBAL DEFAULT       25 _edata
   58: 0000000000000828     44 FUNC    GLOBAL DEFAULT       13 bar
   59: 00000000000008f4      0 FUNC    GLOBAL DEFAULT       14 _fini
   60: 0000000000000000      0 FUNC    GLOBAL DEFAULT    UNDEF __libc_start_main@@GLIBC_2.2.5
   61: 0000000000201030      0 NOTYPE  GLOBAL DEFAULT       25 __data_start
   62: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF __gmon_start__
   63: 0000000000200de8      0 OBJECT  GLOBAL HIDDEN        21 __dso_handle
   64: 0000000000000900      4 OBJECT  GLOBAL DEFAULT       15 _IO_stdin_used
   65: 0000000000201038      4 OBJECT  GLOBAL DEFAULT       25 b2
   66: 0000000000000860    137 FUNC    GLOBAL DEFAULT       13 __libc_csu_init
   67: 0000000000201040      0 NOTYPE  GLOBAL DEFAULT       26 _end
   68: 00000000000006e0      0 FUNC    GLOBAL DEFAULT       13 _start
   69: 000000000020103c      0 NOTYPE  GLOBAL DEFAULT       26 __bss_start
   70: 00000000000007f0     35 FUNC    GLOBAL DEFAULT       13 main
   71: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF _Jv_RegisterClasses
   72: 0000000000201040      0 OBJECT  GLOBAL HIDDEN        25 __TMC_END__
   73: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF _ITM_registerTMCloneTable
   74: 0000000000000000      0 FUNC    WEAK   DEFAULT    UNDEF __cxa_finalize@@GLIBC_2.2.5
   75: 0000000000000680      0 FUNC    GLOBAL DEFAULT       11 _init
EOF

cat > testfile.minsym.in <<\EOF

Symbol table [28] '.symtab' contains 40 entries:
 36 local symbols  String table: [29] '.strtab'
  Num:            Value   Size Type    Bind   Vis          Ndx Name
    0: 0000000000000000      0 NOTYPE  LOCAL  DEFAULT    UNDEF 
    1: 0000000000000710      0 FUNC    LOCAL  DEFAULT       13 deregister_tm_clones
    2: 0000000000000740      0 FUNC    LOCAL  DEFAULT       13 register_tm_clones
    3: 0000000000000780      0 FUNC    LOCAL  DEFAULT       13 __do_global_dtors_aux
    4: 0000000000200dd8      0 OBJECT  LOCAL  DEFAULT       19 __do_global_dtors_aux_fini_array_entry
    5: 00000000000007c0      0 FUNC    LOCAL  DEFAULT       13 frame_dummy
    6: 0000000000200dd0      0 OBJECT  LOCAL  DEFAULT       18 __frame_dummy_init_array_entry
    7: 0000000000000814     20 FUNC    LOCAL  DEFAULT       13 foo
    8: 0000000000200dd8      0 NOTYPE  LOCAL  DEFAULT       18 __init_array_end
    9: 0000000000200dd0      0 NOTYPE  LOCAL  DEFAULT       18 __init_array_start
   10: 0000000000000238      0 SECTION LOCAL  DEFAULT        1 
   11: 0000000000000254      0 SECTION LOCAL  DEFAULT        2 
   12: 0000000000000274      0 SECTION LOCAL  DEFAULT        3 
   13: 0000000000000298      0 SECTION LOCAL  DEFAULT        4 
   14: 00000000000002d8      0 SECTION LOCAL  DEFAULT        5 
   15: 0000000000000428      0 SECTION LOCAL  DEFAULT        6 
   16: 00000000000004f2      0 SECTION LOCAL  DEFAULT        7 
   17: 0000000000000510      0 SECTION LOCAL  DEFAULT        8 
   18: 0000000000000530      0 SECTION LOCAL  DEFAULT        9 
   19: 0000000000000638      0 SECTION LOCAL  DEFAULT       10 
   20: 0000000000000680      0 SECTION LOCAL  DEFAULT       11 
   21: 00000000000006a0      0 SECTION LOCAL  DEFAULT       12 
   22: 00000000000006e0      0 SECTION LOCAL  DEFAULT       13 
   23: 00000000000008f4      0 SECTION LOCAL  DEFAULT       14 
   24: 0000000000000900      0 SECTION LOCAL  DEFAULT       15 
   25: 0000000000000904      0 SECTION LOCAL  DEFAULT       16 
   26: 0000000000000948      0 SECTION LOCAL  DEFAULT       17 
   27: 0000000000200dd0      0 SECTION LOCAL  DEFAULT       18 
   28: 0000000000200dd8      0 SECTION LOCAL  DEFAULT       19 
   29: 0000000000200de0      0 SECTION LOCAL  DEFAULT       20 
   30: 0000000000200de8      0 SECTION LOCAL  DEFAULT       21 
   31: 0000000000200df0      0 SECTION LOCAL  DEFAULT       22 
   32: 0000000000200fc0      0 SECTION LOCAL  DEFAULT       23 
   33: 0000000000201000      0 SECTION LOCAL  DEFAULT       24 
   34: 0000000000201030      0 SECTION LOCAL  DEFAULT       25 
   35: 000000000020103c      0 SECTION LOCAL  DEFAULT       26 
   36: 0000000000000828     44 FUNC    GLOBAL DEFAULT       13 bar
   37: 00000000000008f4      0 FUNC    GLOBAL DEFAULT       14 _fini
   38: 00000000000006e0      0 FUNC    GLOBAL DEFAULT       13 _start
   39: 0000000000000680      0 FUNC    GLOBAL DEFAULT       11 _init
EOF

# Display all symbol tables.
cat testfile.dynsym.in testfile.symtab.in \
  | testrun_compare ${abs_top_builddir}/src/readelf -s testfilebaztab

cat testfile.dynsym.in testfile.symtab.in \
  | testrun_compare ${abs_top_builddir}/src/readelf --syms testfilebaztab

# Display just .dynsym
cat testfile.dynsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf \
    --symbols=.dynsym testfilebaztab
cat testfile.dynsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf \
    --syms=.dynsym testfilebaztab
cat testfile.dynsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf \
    --dyn-syms testfilebaztab

# Display just .symtab
cat testfile.symtab.in \
  | testrun_compare ${abs_top_builddir}/src/readelf \
    --symbols=.symtab testfilebaztab

cat testfile.dynsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf -s testfilebazdbg

cat testfile.symtab.in \
  | testrun_compare ${abs_top_builddir}/src/readelf -s testfilebazdbg.debug

cat testfile.dynsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf -s testfilebazdyn

cat testfile.dynsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf -s testfilebazmdb

cat testfile.minsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf --elf-section -s testfilebazmdb

cat testfile.dynsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf -s testfilebazmin

cat testfile.minsym.in \
  | testrun_compare ${abs_top_builddir}/src/readelf --elf-section -s testfilebazmin

testrun_compare ${abs_top_builddir}/src/readelf -s testfilebasmin <<EOF
EOF

testrun_compare ${abs_top_builddir}/src/readelf --elf-section -s testfilebasmin <<\EOF

Symbol table [ 6] '.symtab' contains 9 entries:
 6 local symbols  String table: [ 7] '.strtab'
  Num:            Value   Size Type    Bind   Vis          Ndx Name
    0: 0000000000000000      0 NOTYPE  LOCAL  DEFAULT    UNDEF 
    1: 0000000000400168     18 FUNC    LOCAL  DEFAULT        2 foo
    2: 0000000000400120      0 SECTION LOCAL  DEFAULT        1 
    3: 0000000000400144      0 SECTION LOCAL  DEFAULT        2 
    4: 00000000004001c0      0 SECTION LOCAL  DEFAULT        3 
    5: 0000000000600258      0 SECTION LOCAL  DEFAULT        4 
    6: 00000000004001a8     21 FUNC    GLOBAL DEFAULT        2 _start
    7: 0000000000400144     33 FUNC    GLOBAL DEFAULT        2 main
    8: 000000000040017a     44 FUNC    GLOBAL DEFAULT        2 bar
EOF

testrun_compare ${abs_top_builddir}/src/readelf -s testfilebaxmin <<EOF

Symbol table [ 5] '.dynsym' contains 3 entries:
 1 local symbol  String table: [ 6] '.dynstr'
  Num:            Value   Size Type    Bind   Vis          Ndx Name
    0: 0000000000000000      0 NOTYPE  LOCAL  DEFAULT    UNDEF 
    1: 0000000000000000      0 FUNC    GLOBAL DEFAULT    UNDEF __libc_start_main@GLIBC_2.2.5 (2)
    2: 0000000000000000      0 NOTYPE  WEAK   DEFAULT    UNDEF __gmon_start__
EOF

testrun_compare ${abs_top_builddir}/src/readelf --elf-section -s testfilebaxmin <<\EOF

Symbol table [27] '.symtab' contains 42 entries:
 35 local symbols  String table: [28] '.strtab'
  Num:            Value   Size Type    Bind   Vis          Ndx Name
    0: 0000000000000000      0 NOTYPE  LOCAL  DEFAULT    UNDEF 
    1: 0000000000400430      0 FUNC    LOCAL  DEFAULT       13 deregister_tm_clones
    2: 0000000000400460      0 FUNC    LOCAL  DEFAULT       13 register_tm_clones
    3: 00000000004004a0      0 FUNC    LOCAL  DEFAULT       13 __do_global_dtors_aux
    4: 0000000000600e18      0 OBJECT  LOCAL  DEFAULT       19 __do_global_dtors_aux_fini_array_entry
    5: 00000000004004c0      0 FUNC    LOCAL  DEFAULT       13 frame_dummy
    6: 0000000000600e10      0 OBJECT  LOCAL  DEFAULT       18 __frame_dummy_init_array_entry
    7: 00000000004004f0     20 FUNC    LOCAL  DEFAULT       13 foo
    8: 0000000000600e18      0 NOTYPE  LOCAL  DEFAULT       18 __init_array_end
    9: 0000000000600e10      0 NOTYPE  LOCAL  DEFAULT       18 __init_array_start
   10: 0000000000400238      0 SECTION LOCAL  DEFAULT        1 
   11: 0000000000400254      0 SECTION LOCAL  DEFAULT        2 
   12: 0000000000400274      0 SECTION LOCAL  DEFAULT        3 
   13: 0000000000400298      0 SECTION LOCAL  DEFAULT        4 
   14: 00000000004002b8      0 SECTION LOCAL  DEFAULT        5 
   15: 0000000000400300      0 SECTION LOCAL  DEFAULT        6 
   16: 0000000000400338      0 SECTION LOCAL  DEFAULT        7 
   17: 0000000000400340      0 SECTION LOCAL  DEFAULT        8 
   18: 0000000000400360      0 SECTION LOCAL  DEFAULT        9 
   19: 0000000000400378      0 SECTION LOCAL  DEFAULT       10 
   20: 00000000004003a8      0 SECTION LOCAL  DEFAULT       11 
   21: 00000000004003d0      0 SECTION LOCAL  DEFAULT       12 
   22: 0000000000400400      0 SECTION LOCAL  DEFAULT       13 
   23: 00000000004005c4      0 SECTION LOCAL  DEFAULT       14 
   24: 00000000004005d0      0 SECTION LOCAL  DEFAULT       15 
   25: 00000000004005e0      0 SECTION LOCAL  DEFAULT       16 
   26: 0000000000400628      0 SECTION LOCAL  DEFAULT       17 
   27: 0000000000600e10      0 SECTION LOCAL  DEFAULT       18 
   28: 0000000000600e18      0 SECTION LOCAL  DEFAULT       19 
   29: 0000000000600e20      0 SECTION LOCAL  DEFAULT       20 
   30: 0000000000600e28      0 SECTION LOCAL  DEFAULT       21 
   31: 0000000000600ff8      0 SECTION LOCAL  DEFAULT       22 
   32: 0000000000601000      0 SECTION LOCAL  DEFAULT       23 
   33: 0000000000601028      0 SECTION LOCAL  DEFAULT       24 
   34: 0000000000601034      0 SECTION LOCAL  DEFAULT       25 
   35: 00000000004005c0      2 FUNC    GLOBAL DEFAULT       13 __libc_csu_fini
   36: 0000000000400504     40 FUNC    GLOBAL DEFAULT       13 bar
   37: 00000000004005c4      0 FUNC    GLOBAL DEFAULT       14 _fini
   38: 0000000000400550    101 FUNC    GLOBAL DEFAULT       13 __libc_csu_init
   39: 0000000000400400      0 FUNC    GLOBAL DEFAULT       13 _start
   40: 000000000040052c     35 FUNC    GLOBAL DEFAULT       13 main
   41: 00000000004003a8      0 FUNC    GLOBAL DEFAULT       11 _init
EOF

testrun ${abs_top_builddir}/src/readelf --elf-section -sW testfilebaxmin 2>&1 \
  | grep "WARNING: cannot find section: 'W'" >/dev/null || exit 2

exit 0
