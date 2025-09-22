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

# Tests dwfl_module_{addrsym,getsym,relocate_address}
# See run-readelf-s.sh for how to generate test binaries.
# In addition, *_pl files were created from their base file
# with prelink -N, and *_plr with prelink -r 0x4200000000.

testfiles testfilebaztab
testfiles testfilebazdbg testfilebazdbg.debug
testfiles testfilebazdbg_pl
testfiles testfilebazdbg_plr
testfiles testfilebazdyn
testfiles testfilebazmdb
testfiles testfilebazmin
testfiles testfilebazmin_pl
testfiles testfilebazmin_plr
testfiles testfilebasmin
testfiles testfilebaxmin

tempfiles testfile.dynsym.in testfile.symtab.in testfile.minsym.in dwflsyms.out
tempfiles testfile.symtab_pl.in testfile.minsym_pl.in 

cat > testfile.symtab.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x238
   2: SECTION	LOCAL	 (0) 0x254
   3: SECTION	LOCAL	 (0) 0x274
   4: SECTION	LOCAL	 (0) 0x298
   5: SECTION	LOCAL	 (0) 0x2d8
   6: SECTION	LOCAL	 (0) 0x428
   7: SECTION	LOCAL	 (0) 0x4f2
   8: SECTION	LOCAL	 (0) 0x510
   9: SECTION	LOCAL	 (0) 0x530
  10: SECTION	LOCAL	 (0) 0x638
  11: SECTION	LOCAL	 (0) 0x680
  12: SECTION	LOCAL	 (0) 0x6a0
  13: SECTION	LOCAL	 (0) 0x6e0
  14: SECTION	LOCAL	 (0) 0x8f4
  15: SECTION	LOCAL	 (0) 0x900
  16: SECTION	LOCAL	 (0) 0x904
  17: SECTION	LOCAL	 (0) 0x948
  18: SECTION	LOCAL	 (0) 0x200dd0
  19: SECTION	LOCAL	 (0) 0x200dd8
  20: SECTION	LOCAL	 (0) 0x200de0
  21: SECTION	LOCAL	 (0) 0x200de8
  22: SECTION	LOCAL	 (0) 0x200df0
  23: SECTION	LOCAL	 (0) 0x200fc0
  24: SECTION	LOCAL	 (0) 0x201000
  25: SECTION	LOCAL	 (0) 0x201030
  26: SECTION	LOCAL	 (0) 0x20103c
  27: SECTION	LOCAL	 (0) 0
  28: SECTION	LOCAL	 (0) 0
  29: SECTION	LOCAL	 (0) 0
  30: SECTION	LOCAL	 (0) 0
  31: SECTION	LOCAL	 (0) 0
  32: SECTION	LOCAL	 (0) 0
  33: FILE	LOCAL	crtstuff.c (0) 0
  34: OBJECT	LOCAL	__JCR_LIST__ (0) 0x200de0
  35: FUNC	LOCAL	deregister_tm_clones (0) 0x710, rel: 0x710 (.text)
  36: FUNC	LOCAL	register_tm_clones (0) 0x740, rel: 0x740 (.text)
  37: FUNC	LOCAL	__do_global_dtors_aux (0) 0x780, rel: 0x780 (.text)
  38: OBJECT	LOCAL	completed.6137 (1) 0x20103c
  39: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x200dd8
  40: FUNC	LOCAL	frame_dummy (0) 0x7c0, rel: 0x7c0 (.text)
  41: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x200dd0
  42: FILE	LOCAL	foo.c (0) 0
  43: FILE	LOCAL	bar.c (0) 0
  44: OBJECT	LOCAL	b1 (4) 0x201034
  45: FUNC	LOCAL	foo (20) 0x814, rel: 0x814 (.text)
  46: FILE	LOCAL	crtstuff.c (0) 0
  47: OBJECT	LOCAL	__FRAME_END__ (0) 0xa58
  48: OBJECT	LOCAL	__JCR_END__ (0) 0x200de0
  49: FILE	LOCAL	 (0) 0
  50: NOTYPE	LOCAL	__init_array_end (0) 0x200dd8
  51: OBJECT	LOCAL	_DYNAMIC (0) 0x200df0
  52: NOTYPE	LOCAL	__init_array_start (0) 0x200dd0
  53: OBJECT	LOCAL	_GLOBAL_OFFSET_TABLE_ (0) 0x201000
  54: FUNC	GLOBAL	__libc_csu_fini (2) 0x8f0, rel: 0x8f0 (.text)
  55: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  56: NOTYPE	WEAK	data_start (0) 0x201030
  57: NOTYPE	GLOBAL	_edata (0) 0x20103c
  58: FUNC	GLOBAL	bar (44) 0x828, rel: 0x828 (.text)
  59: FUNC	GLOBAL	_fini (0) 0x8f4, rel: 0x8f4 (.fini)
  60: FUNC	GLOBAL	__libc_start_main@@GLIBC_2.2.5 (0) 0
  61: NOTYPE	GLOBAL	__data_start (0) 0x201030
  62: NOTYPE	WEAK	__gmon_start__ (0) 0
  63: OBJECT	GLOBAL	__dso_handle (0) 0x200de8
  64: OBJECT	GLOBAL	_IO_stdin_used (4) 0x900
  65: OBJECT	GLOBAL	b2 (4) 0x201038
  66: FUNC	GLOBAL	__libc_csu_init (137) 0x860, rel: 0x860 (.text)
  67: NOTYPE	GLOBAL	_end (0) 0x201040
  68: FUNC	GLOBAL	_start (0) 0x6e0, rel: 0x6e0 (.text)
  69: NOTYPE	GLOBAL	__bss_start (0) 0x20103c
  70: FUNC	GLOBAL	main (35) 0x7f0, rel: 0x7f0 (.text)
  71: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  72: OBJECT	GLOBAL	__TMC_END__ (0) 0x201040
  73: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  74: FUNC	WEAK	__cxa_finalize@@GLIBC_2.2.5 (0) 0
  75: FUNC	GLOBAL	_init (0) 0x680, rel: 0x680 (.init)
EOF

cat > testfile.symtab_pl.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x3000000238
   2: SECTION	LOCAL	 (0) 0x3000000254
   3: SECTION	LOCAL	 (0) 0x3000000274
   4: SECTION	LOCAL	 (0) 0x3000000298
   5: SECTION	LOCAL	 (0) 0x30000002d8
   6: SECTION	LOCAL	 (0) 0x3000000428
   7: SECTION	LOCAL	 (0) 0x30000004f2
   8: SECTION	LOCAL	 (0) 0x3000000510
   9: SECTION	LOCAL	 (0) 0x3000000530
  10: SECTION	LOCAL	 (0) 0x3000000638
  11: SECTION	LOCAL	 (0) 0x3000000680
  12: SECTION	LOCAL	 (0) 0x30000006a0
  13: SECTION	LOCAL	 (0) 0x30000006e0
  14: SECTION	LOCAL	 (0) 0x30000008f4
  15: SECTION	LOCAL	 (0) 0x3000000900
  16: SECTION	LOCAL	 (0) 0x3000000904
  17: SECTION	LOCAL	 (0) 0x3000000948
  18: SECTION	LOCAL	 (0) 0x3000200dd0
  19: SECTION	LOCAL	 (0) 0x3000200dd8
  20: SECTION	LOCAL	 (0) 0x3000200de0
  21: SECTION	LOCAL	 (0) 0x3000200de8
  22: SECTION	LOCAL	 (0) 0x3000200df0
  23: SECTION	LOCAL	 (0) 0x3000200fc0
  24: SECTION	LOCAL	 (0) 0x3000201000
  25: SECTION	LOCAL	 (0) 0x3000201030
  26: SECTION	LOCAL	 (0) 0x300020103c
  27: SECTION	LOCAL	 (0) 0
  28: SECTION	LOCAL	 (0) 0
  29: SECTION	LOCAL	 (0) 0
  30: SECTION	LOCAL	 (0) 0
  31: SECTION	LOCAL	 (0) 0
  32: SECTION	LOCAL	 (0) 0
  33: FILE	LOCAL	crtstuff.c (0) 0
  34: OBJECT	LOCAL	__JCR_LIST__ (0) 0x3000200de0
  35: FUNC	LOCAL	deregister_tm_clones (0) 0x3000000710, rel: 0x710 (.text)
  36: FUNC	LOCAL	register_tm_clones (0) 0x3000000740, rel: 0x740 (.text)
  37: FUNC	LOCAL	__do_global_dtors_aux (0) 0x3000000780, rel: 0x780 (.text)
  38: OBJECT	LOCAL	completed.6137 (1) 0x300020103c
  39: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x3000200dd8
  40: FUNC	LOCAL	frame_dummy (0) 0x30000007c0, rel: 0x7c0 (.text)
  41: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x3000200dd0
  42: FILE	LOCAL	foo.c (0) 0
  43: FILE	LOCAL	bar.c (0) 0
  44: OBJECT	LOCAL	b1 (4) 0x3000201034
  45: FUNC	LOCAL	foo (20) 0x3000000814, rel: 0x814 (.text)
  46: FILE	LOCAL	crtstuff.c (0) 0
  47: OBJECT	LOCAL	__FRAME_END__ (0) 0x3000000a58
  48: OBJECT	LOCAL	__JCR_END__ (0) 0x3000200de0
  49: FILE	LOCAL	 (0) 0
  50: NOTYPE	LOCAL	__init_array_end (0) 0x3000200dd8
  51: OBJECT	LOCAL	_DYNAMIC (0) 0x3000200df0
  52: NOTYPE	LOCAL	__init_array_start (0) 0x3000200dd0
  53: OBJECT	LOCAL	_GLOBAL_OFFSET_TABLE_ (0) 0x3000201000
  54: FUNC	GLOBAL	__libc_csu_fini (2) 0x30000008f0, rel: 0x8f0 (.text)
  55: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  56: NOTYPE	WEAK	data_start (0) 0x3000201030
  57: NOTYPE	GLOBAL	_edata (0) 0x300020103c
  58: FUNC	GLOBAL	bar (44) 0x3000000828, rel: 0x828 (.text)
  59: FUNC	GLOBAL	_fini (0) 0x30000008f4, rel: 0x8f4 (.fini)
  60: FUNC	GLOBAL	__libc_start_main@@GLIBC_2.2.5 (0) 0
  61: NOTYPE	GLOBAL	__data_start (0) 0x3000201030
  62: NOTYPE	WEAK	__gmon_start__ (0) 0
  63: OBJECT	GLOBAL	__dso_handle (0) 0x3000200de8
  64: OBJECT	GLOBAL	_IO_stdin_used (4) 0x3000000900
  65: OBJECT	GLOBAL	b2 (4) 0x3000201038
  66: FUNC	GLOBAL	__libc_csu_init (137) 0x3000000860, rel: 0x860 (.text)
  67: NOTYPE	GLOBAL	_end (0) 0x3000201040
  68: FUNC	GLOBAL	_start (0) 0x30000006e0, rel: 0x6e0 (.text)
  69: NOTYPE	GLOBAL	__bss_start (0) 0x300020103c
  70: FUNC	GLOBAL	main (35) 0x30000007f0, rel: 0x7f0 (.text)
  71: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  72: OBJECT	GLOBAL	__TMC_END__ (0) 0x3000201040
  73: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  74: FUNC	WEAK	__cxa_finalize@@GLIBC_2.2.5 (0) 0
  75: FUNC	GLOBAL	_init (0) 0x3000000680, rel: 0x680 (.init)
EOF

cat > testfile.dynsym.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x238
   2: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
   3: FUNC	GLOBAL	__libc_start_main (0) 0
   4: NOTYPE	WEAK	__gmon_start__ (0) 0
   5: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
   6: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
   7: FUNC	WEAK	__cxa_finalize (0) 0
   8: NOTYPE	GLOBAL	_edata (0) 0x20103c
   9: NOTYPE	GLOBAL	_end (0) 0x201040
  10: FUNC	GLOBAL	__libc_csu_init (137) 0x860, rel: 0x860 (.text)
  11: NOTYPE	GLOBAL	__bss_start (0) 0x20103c
  12: FUNC	GLOBAL	main (35) 0x7f0, rel: 0x7f0 (.text)
  13: FUNC	GLOBAL	__libc_csu_fini (2) 0x8f0, rel: 0x8f0 (.text)
EOF

cat > testfile.minsym.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x238
   2: FUNC	LOCAL	deregister_tm_clones (0) 0x710, rel: 0x710 (.text)
   3: FUNC	LOCAL	register_tm_clones (0) 0x740, rel: 0x740 (.text)
   4: FUNC	LOCAL	__do_global_dtors_aux (0) 0x780, rel: 0x780 (.text)
   5: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x200dd8
   6: FUNC	LOCAL	frame_dummy (0) 0x7c0, rel: 0x7c0 (.text)
   7: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x200dd0
   8: FUNC	LOCAL	foo (20) 0x814, rel: 0x814 (.text)
   9: NOTYPE	LOCAL	__init_array_end (0) 0x200dd8
  10: NOTYPE	LOCAL	__init_array_start (0) 0x200dd0
  11: SECTION	LOCAL	 (0) 0x238
  12: SECTION	LOCAL	 (0) 0x254
  13: SECTION	LOCAL	 (0) 0x274
  14: SECTION	LOCAL	 (0) 0x298
  15: SECTION	LOCAL	 (0) 0x2d8
  16: SECTION	LOCAL	 (0) 0x428
  17: SECTION	LOCAL	 (0) 0x4f2
  18: SECTION	LOCAL	 (0) 0x510
  19: SECTION	LOCAL	 (0) 0x530
  20: SECTION	LOCAL	 (0) 0x638
  21: SECTION	LOCAL	 (0) 0x680
  22: SECTION	LOCAL	 (0) 0x6a0
  23: SECTION	LOCAL	 (0) 0x6e0
  24: SECTION	LOCAL	 (0) 0x8f4
  25: SECTION	LOCAL	 (0) 0x900
  26: SECTION	LOCAL	 (0) 0x904
  27: SECTION	LOCAL	 (0) 0x948
  28: SECTION	LOCAL	 (0) 0x200dd0
  29: SECTION	LOCAL	 (0) 0x200dd8
  30: SECTION	LOCAL	 (0) 0x200de0
  31: SECTION	LOCAL	 (0) 0x200de8
  32: SECTION	LOCAL	 (0) 0x200df0
  33: SECTION	LOCAL	 (0) 0x200fc0
  34: SECTION	LOCAL	 (0) 0x201000
  35: SECTION	LOCAL	 (0) 0x201030
  36: SECTION	LOCAL	 (0) 0x20103c
  37: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  38: FUNC	GLOBAL	__libc_start_main (0) 0
  39: NOTYPE	WEAK	__gmon_start__ (0) 0
  40: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  41: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  42: FUNC	WEAK	__cxa_finalize (0) 0
  43: NOTYPE	GLOBAL	_edata (0) 0x20103c
  44: NOTYPE	GLOBAL	_end (0) 0x201040
  45: FUNC	GLOBAL	__libc_csu_init (137) 0x860, rel: 0x860 (.text)
  46: NOTYPE	GLOBAL	__bss_start (0) 0x20103c
  47: FUNC	GLOBAL	main (35) 0x7f0, rel: 0x7f0 (.text)
  48: FUNC	GLOBAL	__libc_csu_fini (2) 0x8f0, rel: 0x8f0 (.text)
  49: FUNC	GLOBAL	bar (44) 0x828, rel: 0x828 (.text)
  50: FUNC	GLOBAL	_fini (0) 0x8f4, rel: 0x8f4 (.fini)
  51: FUNC	GLOBAL	_start (0) 0x6e0, rel: 0x6e0 (.text)
  52: FUNC	GLOBAL	_init (0) 0x680, rel: 0x680 (.init)
EOF

cat > testfile.minsym_pl.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x3000000238
   2: FUNC	LOCAL	deregister_tm_clones (0) 0x3000000710, rel: 0x710 (.text)
   3: FUNC	LOCAL	register_tm_clones (0) 0x3000000740, rel: 0x740 (.text)
   4: FUNC	LOCAL	__do_global_dtors_aux (0) 0x3000000780, rel: 0x780 (.text)
   5: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x3000200dd8
   6: FUNC	LOCAL	frame_dummy (0) 0x30000007c0, rel: 0x7c0 (.text)
   7: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x3000200dd0
   8: FUNC	LOCAL	foo (20) 0x3000000814, rel: 0x814 (.text)
   9: NOTYPE	LOCAL	__init_array_end (0) 0x3000200dd8
  10: NOTYPE	LOCAL	__init_array_start (0) 0x3000200dd0
  11: SECTION	LOCAL	 (0) 0x3000000238
  12: SECTION	LOCAL	 (0) 0x3000000254
  13: SECTION	LOCAL	 (0) 0x3000000274
  14: SECTION	LOCAL	 (0) 0x3000000298
  15: SECTION	LOCAL	 (0) 0x30000002d8
  16: SECTION	LOCAL	 (0) 0x3000000428
  17: SECTION	LOCAL	 (0) 0x30000004f2
  18: SECTION	LOCAL	 (0) 0x3000000510
  19: SECTION	LOCAL	 (0) 0x3000000530
  20: SECTION	LOCAL	 (0) 0x3000000638
  21: SECTION	LOCAL	 (0) 0x3000000680
  22: SECTION	LOCAL	 (0) 0x30000006a0
  23: SECTION	LOCAL	 (0) 0x30000006e0
  24: SECTION	LOCAL	 (0) 0x30000008f4
  25: SECTION	LOCAL	 (0) 0x3000000900
  26: SECTION	LOCAL	 (0) 0x3000000904
  27: SECTION	LOCAL	 (0) 0x3000000948
  28: SECTION	LOCAL	 (0) 0x3000200dd0
  29: SECTION	LOCAL	 (0) 0x3000200dd8
  30: SECTION	LOCAL	 (0) 0x3000200de0
  31: SECTION	LOCAL	 (0) 0x3000200de8
  32: SECTION	LOCAL	 (0) 0x3000200df0
  33: SECTION	LOCAL	 (0) 0x3000200fc0
  34: SECTION	LOCAL	 (0) 0x3000201000
  35: SECTION	LOCAL	 (0) 0x3000201030
  36: SECTION	LOCAL	 (0) 0x300020103c
  37: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  38: FUNC	GLOBAL	__libc_start_main (0) 0
  39: NOTYPE	WEAK	__gmon_start__ (0) 0
  40: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  41: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  42: FUNC	WEAK	__cxa_finalize (0) 0
  43: NOTYPE	GLOBAL	_edata (0) 0x300020103c
  44: NOTYPE	GLOBAL	_end (0) 0x3000201040
  45: FUNC	GLOBAL	__libc_csu_init (137) 0x3000000860, rel: 0x860 (.text)
  46: NOTYPE	GLOBAL	__bss_start (0) 0x300020103c
  47: FUNC	GLOBAL	main (35) 0x30000007f0, rel: 0x7f0 (.text)
  48: FUNC	GLOBAL	__libc_csu_fini (2) 0x30000008f0, rel: 0x8f0 (.text)
  49: FUNC	GLOBAL	bar (44) 0x3000000828, rel: 0x828 (.text)
  50: FUNC	GLOBAL	_fini (0) 0x30000008f4, rel: 0x8f4 (.fini)
  51: FUNC	GLOBAL	_start (0) 0x30000006e0, rel: 0x6e0 (.text)
  52: FUNC	GLOBAL	_init (0) 0x3000000680, rel: 0x680 (.init)
EOF

cat testfile.symtab.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebaztab

cat testfile.symtab.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdbg

cat testfile.symtab_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdbg_pl

sed s/0x3000/0x4200/g testfile.symtab_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdbg_plr

cat testfile.dynsym.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdyn

cat testfile.symtab.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazmdb

cat testfile.minsym.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazmin

cat testfile.minsym_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazmin_pl

sed s/0x3000/0x4200/g testfile.minsym_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazmin_plr

testrun_compare ${abs_builddir}/dwflsyms -e testfilebasmin <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: FUNC	LOCAL	foo (18) 0x400168, rel: 0x400168 (.text)
   2: SECTION	LOCAL	 (0) 0x400120
   3: SECTION	LOCAL	 (0) 0x400144
   4: SECTION	LOCAL	 (0) 0x4001c0
   5: SECTION	LOCAL	 (0) 0x600258
   6: FUNC	GLOBAL	_start (21) 0x4001a8, rel: 0x4001a8 (.text)
   7: FUNC	GLOBAL	main (33) 0x400144, rel: 0x400144 (.text)
   8: FUNC	GLOBAL	bar (44) 0x40017a, rel: 0x40017a (.text)
EOF

testrun_compare ${abs_builddir}/dwflsyms -e testfilebaxmin <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: FUNC	LOCAL	deregister_tm_clones (0) 0x400430, rel: 0x400430 (.text)
   2: FUNC	LOCAL	register_tm_clones (0) 0x400460, rel: 0x400460 (.text)
   3: FUNC	LOCAL	__do_global_dtors_aux (0) 0x4004a0, rel: 0x4004a0 (.text)
   4: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x600e18
   5: FUNC	LOCAL	frame_dummy (0) 0x4004c0, rel: 0x4004c0 (.text)
   6: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x600e10
   7: FUNC	LOCAL	foo (20) 0x4004f0, rel: 0x4004f0 (.text)
   8: NOTYPE	LOCAL	__init_array_end (0) 0x600e18
   9: NOTYPE	LOCAL	__init_array_start (0) 0x600e10
  10: SECTION	LOCAL	 (0) 0x400238
  11: SECTION	LOCAL	 (0) 0x400254
  12: SECTION	LOCAL	 (0) 0x400274
  13: SECTION	LOCAL	 (0) 0x400298
  14: SECTION	LOCAL	 (0) 0x4002b8
  15: SECTION	LOCAL	 (0) 0x400300
  16: SECTION	LOCAL	 (0) 0x400338
  17: SECTION	LOCAL	 (0) 0x400340
  18: SECTION	LOCAL	 (0) 0x400360
  19: SECTION	LOCAL	 (0) 0x400378
  20: SECTION	LOCAL	 (0) 0x4003a8
  21: SECTION	LOCAL	 (0) 0x4003d0
  22: SECTION	LOCAL	 (0) 0x400400
  23: SECTION	LOCAL	 (0) 0x4005c4
  24: SECTION	LOCAL	 (0) 0x4005d0
  25: SECTION	LOCAL	 (0) 0x4005e0
  26: SECTION	LOCAL	 (0) 0x400628
  27: SECTION	LOCAL	 (0) 0x600e10
  28: SECTION	LOCAL	 (0) 0x600e18
  29: SECTION	LOCAL	 (0) 0x600e20
  30: SECTION	LOCAL	 (0) 0x600e28
  31: SECTION	LOCAL	 (0) 0x600ff8
  32: SECTION	LOCAL	 (0) 0x601000
  33: SECTION	LOCAL	 (0) 0x601028
  34: SECTION	LOCAL	 (0) 0x601034
  35: FUNC	GLOBAL	__libc_start_main (0) 0
  36: NOTYPE	WEAK	__gmon_start__ (0) 0
  37: FUNC	GLOBAL	__libc_csu_fini (2) 0x4005c0, rel: 0x4005c0 (.text)
  38: FUNC	GLOBAL	bar (40) 0x400504, rel: 0x400504 (.text)
  39: FUNC	GLOBAL	_fini (0) 0x4005c4, rel: 0x4005c4 (.fini)
  40: FUNC	GLOBAL	__libc_csu_init (101) 0x400550, rel: 0x400550 (.text)
  41: FUNC	GLOBAL	_start (0) 0x400400, rel: 0x400400 (.text)
  42: FUNC	GLOBAL	main (35) 0x40052c, rel: 0x40052c (.text)
  43: FUNC	GLOBAL	_init (0) 0x4003a8, rel: 0x4003a8 (.init)
EOF

testfiles testfile66
testrun_compare ${abs_builddir}/dwflsyms -e testfile66 <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x190
   2: SECTION	LOCAL	 (0) 0x1a4
   3: SECTION	LOCAL	 (0) 0x1c8
   4: SECTION	LOCAL	 (0) 0x1f8
   5: SECTION	LOCAL	 (0) 0x288
   6: SECTION	LOCAL	 (0) 0x2a8
   7: SECTION	LOCAL	 (0) 0x2d8
   8: SECTION	LOCAL	 (0) 0x102e0
   9: SECTION	LOCAL	 (0) 0x103d0
  10: SECTION	LOCAL	 (0) 0x103e8
  11: SECTION	LOCAL	 (0) 0x103e8
  12: OBJECT	LOCAL	_DYNAMIC (0) 0x102e0
  13: FUNC	GLOBAL	_start (4) 0x103d0, rel: 0x103d0 (.opd) [0x2d8, rel: 0 (.text)]
  14: NOTYPE	GLOBAL	__bss_start (0) 0x103f0
  15: NOTYPE	GLOBAL	_edata (0) 0x103f0
  16: NOTYPE	GLOBAL	_end (0) 0x103f0
EOF

testfiles testfile66.core
testrun_compare ${abs_builddir}/dwflsyms -e testfile66 --core=testfile66.core <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0xfffb1af0410
   2: NOTYPE	GLOBAL	__kernel_datapage_offset (0) 0xfffb1af05dc
   3: OBJECT	GLOBAL	LINUX_2.6.15 (0) 0
   4: NOTYPE	GLOBAL	__kernel_clock_getres (64) 0xfffb1af052c
   5: NOTYPE	GLOBAL	__kernel_get_tbfreq (24) 0xfffb1af0620
   6: NOTYPE	GLOBAL	__kernel_gettimeofday (84) 0xfffb1af0440
   7: NOTYPE	GLOBAL	__kernel_sync_dicache (20) 0xfffb1af06c4
   8: NOTYPE	GLOBAL	__kernel_sync_dicache_p5 (20) 0xfffb1af06c4
   9: NOTYPE	GLOBAL	__kernel_sigtramp_rt64 (12) 0xfffb1af0418
  10: NOTYPE	GLOBAL	__kernel_clock_gettime (152) 0xfffb1af0494
  11: NOTYPE	GLOBAL	__kernel_get_syscall_map (44) 0xfffb1af05f4
ld64.so.1: Callback returned failure
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x461b0190
   2: SECTION	LOCAL	 (0) 0x461b01a4
   3: SECTION	LOCAL	 (0) 0x461b01c8
   4: SECTION	LOCAL	 (0) 0x461b01f8
   5: SECTION	LOCAL	 (0) 0x461b0288
   6: SECTION	LOCAL	 (0) 0x461b02a8
   7: SECTION	LOCAL	 (0) 0x461b02d8
   8: SECTION	LOCAL	 (0) 0x461c02e0
   9: SECTION	LOCAL	 (0) 0x461c03d0
  10: SECTION	LOCAL	 (0) 0x461c03e8
  11: SECTION	LOCAL	 (0) 0x461c03e8
  12: OBJECT	LOCAL	_DYNAMIC (0) 0x102e0
  13: FUNC	GLOBAL	_start (4) 0x461c03d0, rel: 0x103d0 (.opd) [0x461b02d8, rel: 0 (.text)]
  14: NOTYPE	GLOBAL	__bss_start (0) 0x103f0
  15: NOTYPE	GLOBAL	_edata (0) 0x103f0
  16: NOTYPE	GLOBAL	_end (0) 0x103f0
EOF

# Test the already present dot-prefixed names do not get duplicated.
testfiles hello_ppc64.ko
testrun_compare ${abs_builddir}/dwflsyms -e hello_ppc64.ko <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0
   2: SECTION	LOCAL	 (0) 0x94
   3: SECTION	LOCAL	 (0) 0xba
   4: SECTION	LOCAL	 (0) 0xd0
   5: SECTION	LOCAL	 (0) 0x13a
   6: SECTION	LOCAL	 (0) 0x13a
   7: SECTION	LOCAL	 (0) 0x150
   8: SECTION	LOCAL	 (0) 0x170
   9: SECTION	LOCAL	 (0) 0x188
  10: SECTION	LOCAL	 (0) 0x410
  11: SECTION	LOCAL	 (0) 0x434
  12: SECTION	LOCAL	 (0) 0x438
  13: SECTION	LOCAL	 (0) 0x438
  14: SECTION	LOCAL	 (0) 0
  15: SECTION	LOCAL	 (0) 0
  16: SECTION	LOCAL	 (0) 0
  17: SECTION	LOCAL	 (0) 0
  18: SECTION	LOCAL	 (0) 0
  19: SECTION	LOCAL	 (0) 0
  20: SECTION	LOCAL	 (0) 0
  21: SECTION	LOCAL	 (0) 0
  22: SECTION	LOCAL	 (0) 0
  23: SECTION	LOCAL	 (0) 0
  24: FILE	LOCAL	init.c (0) 0
  25: FILE	LOCAL	exit.c (0) 0
  26: FILE	LOCAL	hello.mod.c (0) 0
  27: OBJECT	LOCAL	__mod_srcversion23 (35) 0xd0
  28: OBJECT	LOCAL	__module_depends (9) 0xf8
  29: OBJECT	LOCAL	__mod_vermagic5 (50) 0x108
  30: OBJECT	GLOBAL	__this_module (648) 0x188
  31: FUNC	GLOBAL	.cleanup_module (72) 0x4c, rel: 0x4c (.text)
  32: FUNC	GLOBAL	cleanup_module (24) 0x160, rel: 0x10 (.opd)
  33: NOTYPE	GLOBAL	.printk (0) 0
  34: FUNC	GLOBAL	init_module (24) 0x150, rel: 0 (.opd)
  35: NOTYPE	GLOBAL	._mcount (0) 0
  36: FUNC	GLOBAL	.init_module (76) 0, rel: 0 (.text)
  37: NOTYPE	GLOBAL	_mcount (0) 0
EOF

# Same test files as above, but now generated on ppc64.
# ppc64 uses function descriptors to make things more "interesting".

testfiles testfilebaztabppc64
testfiles testfilebazdbgppc64 testfilebazdbgppc64.debug
testfiles testfilebazdbgppc64_pl
testfiles testfilebazdbgppc64_plr
testfiles testfilebazdynppc64
testfiles testfilebazmdbppc64
testfiles testfilebazminppc64
testfiles testfilebazminppc64_pl
testfiles testfilebazminppc64_plr

cat > testfile.symtab.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x238
   2: SECTION	LOCAL	 (0) 0x24c
   3: SECTION	LOCAL	 (0) 0x26c
   4: SECTION	LOCAL	 (0) 0x290
   5: SECTION	LOCAL	 (0) 0x2c0
   6: SECTION	LOCAL	 (0) 0x3e0
   7: SECTION	LOCAL	 (0) 0x488
   8: SECTION	LOCAL	 (0) 0x4a0
   9: SECTION	LOCAL	 (0) 0x4c0
  10: SECTION	LOCAL	 (0) 0x820
  11: SECTION	LOCAL	 (0) 0x850
  12: SECTION	LOCAL	 (0) 0x8a0
  13: SECTION	LOCAL	 (0) 0xd30
  14: SECTION	LOCAL	 (0) 0xd4c
  15: SECTION	LOCAL	 (0) 0xd50
  16: SECTION	LOCAL	 (0) 0xd70
  17: SECTION	LOCAL	 (0) 0x1fde0
  18: SECTION	LOCAL	 (0) 0x1fde8
  19: SECTION	LOCAL	 (0) 0x1fdf0
  20: SECTION	LOCAL	 (0) 0x1fdf8
  21: SECTION	LOCAL	 (0) 0x1fe20
  22: SECTION	LOCAL	 (0) 0x20000
  23: SECTION	LOCAL	 (0) 0x20010
  24: SECTION	LOCAL	 (0) 0x200d8
  25: SECTION	LOCAL	 (0) 0x20110
  26: SECTION	LOCAL	 (0) 0x20158
  27: SECTION	LOCAL	 (0) 0
  28: SECTION	LOCAL	 (0) 0
  29: SECTION	LOCAL	 (0) 0
  30: SECTION	LOCAL	 (0) 0
  31: SECTION	LOCAL	 (0) 0
  32: SECTION	LOCAL	 (0) 0
  33: SECTION	LOCAL	 (0) 0
  34: FILE	LOCAL	crtstuff.c (0) 0
  35: OBJECT	LOCAL	__JCR_LIST__ (0) 0x1fdf0
  36: FUNC	LOCAL	deregister_tm_clones (0) 0x20040, rel: 0x20040 (.opd) [0x910, rel: 0x70 (.text)]
  37: FUNC	LOCAL	register_tm_clones (0) 0x20050, rel: 0x20050 (.opd) [0x980, rel: 0xe0 (.text)]
  38: FUNC	LOCAL	__do_global_dtors_aux (0) 0x20060, rel: 0x20060 (.opd) [0x9f0, rel: 0x150 (.text)]
  39: OBJECT	LOCAL	completed.7711 (1) 0x20158
  40: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x1fde8
  41: FUNC	LOCAL	frame_dummy (0) 0x20070, rel: 0x20070 (.opd) [0xa50, rel: 0x1b0 (.text)]
  42: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x1fde0
  43: FILE	LOCAL	foo.c (0) 0
  44: FILE	LOCAL	bar.c (0) 0
  45: OBJECT	LOCAL	b1 (4) 0x20004
  46: FUNC	LOCAL	foo (76) 0x20090, rel: 0x20090 (.opd) [0xb34, rel: 0x294 (.text)]
  47: FILE	LOCAL	crtstuff.c (0) 0
  48: OBJECT	LOCAL	__FRAME_END__ (0) 0xe18
  49: OBJECT	LOCAL	__JCR_END__ (0) 0x1fdf0
  50: FILE	LOCAL	 (0) 0
  51: NOTYPE	LOCAL	__glink_PLTresolve (0) 0xce8
  52: NOTYPE	LOCAL	00000011.plt_call.__libc_start_main@@GLIBC_2.3 (0) 0x8a0
  53: NOTYPE	LOCAL	00000011.plt_call.__cxa_finalize@@GLIBC_2.3 (0) 0x8b4
  54: NOTYPE	LOCAL	__init_array_end (0) 0x1fde8
  55: OBJECT	LOCAL	_DYNAMIC (0) 0x1fe20
  56: NOTYPE	LOCAL	__init_array_start (0) 0x1fde0
  57: FUNC	GLOBAL	__libc_csu_fini (16) 0x200c0, rel: 0x200c0 (.opd) [0xcd0, rel: 0x430 (.text)]
  58: FUNC	GLOBAL	__libc_start_main@@GLIBC_2.3 (0) 0
  59: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  60: NOTYPE	WEAK	data_start (0) 0x20000
  61: NOTYPE	GLOBAL	_edata (0) 0x20110
  62: FUNC	GLOBAL	bar (116) 0x200a0, rel: 0x200a0 (.opd) [0xb80, rel: 0x2e0 (.text)]
  63: FUNC	GLOBAL	_fini (0) 0x20030, rel: 0x20030 (.opd) [0xd30, rel: 0 (.fini)]
  64: NOTYPE	GLOBAL	__data_start (0) 0x20000
  65: NOTYPE	WEAK	__gmon_start__ (0) 0
  66: OBJECT	GLOBAL	__dso_handle (0) 0x1fe18
  67: OBJECT	GLOBAL	_IO_stdin_used (4) 0xd4c
  68: OBJECT	GLOBAL	b2 (4) 0x20008
  69: FUNC	WEAK	__cxa_finalize@@GLIBC_2.3 (0) 0
  70: FUNC	GLOBAL	__libc_csu_init (204) 0x200b0, rel: 0x200b0 (.opd) [0xc00, rel: 0x360 (.text)]
  71: NOTYPE	GLOBAL	_end (0) 0x20160
  72: FUNC	GLOBAL	_start (60) 0x20010, rel: 0x20010 (.opd) [0x8c8, rel: 0x28 (.text)]
  73: NOTYPE	GLOBAL	__bss_start (0) 0x20110
  74: FUNC	GLOBAL	main (128) 0x20080, rel: 0x20080 (.opd) [0xab4, rel: 0x214 (.text)]
  75: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  76: OBJECT	GLOBAL	__TMC_END__ (0) 0x20010
  77: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  78: FUNC	GLOBAL	_init (0) 0x20020, rel: 0x20020 (.opd) [0x850, rel: 0 (.init)]
EOF

cat > testfile.symtab_pl.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x8001000238
   2: SECTION	LOCAL	 (0) 0x800100024c
   3: SECTION	LOCAL	 (0) 0x800100026c
   4: SECTION	LOCAL	 (0) 0x8001000290
   5: SECTION	LOCAL	 (0) 0x80010002c0
   6: SECTION	LOCAL	 (0) 0x80010003e0
   7: SECTION	LOCAL	 (0) 0x8001000488
   8: SECTION	LOCAL	 (0) 0x80010004a0
   9: SECTION	LOCAL	 (0) 0x80010004c0
  10: SECTION	LOCAL	 (0) 0x8001000820
  11: SECTION	LOCAL	 (0) 0x8001000850
  12: SECTION	LOCAL	 (0) 0x80010008a0
  13: SECTION	LOCAL	 (0) 0x8001000d30
  14: SECTION	LOCAL	 (0) 0x8001000d4c
  15: SECTION	LOCAL	 (0) 0x8001000d50
  16: SECTION	LOCAL	 (0) 0x8001000d70
  17: SECTION	LOCAL	 (0) 0x800101fde0
  18: SECTION	LOCAL	 (0) 0x800101fde8
  19: SECTION	LOCAL	 (0) 0x800101fdf0
  20: SECTION	LOCAL	 (0) 0x800101fdf8
  21: SECTION	LOCAL	 (0) 0x800101fe20
  22: SECTION	LOCAL	 (0) 0x8001020000
  23: SECTION	LOCAL	 (0) 0x8001020010
  24: SECTION	LOCAL	 (0) 0x80010200d8
  25: SECTION	LOCAL	 (0) 0x8001020110
  26: SECTION	LOCAL	 (0) 0x8001020158
  27: SECTION	LOCAL	 (0) 0
  28: SECTION	LOCAL	 (0) 0
  29: SECTION	LOCAL	 (0) 0
  30: SECTION	LOCAL	 (0) 0
  31: SECTION	LOCAL	 (0) 0
  32: SECTION	LOCAL	 (0) 0
  33: SECTION	LOCAL	 (0) 0
  34: FILE	LOCAL	crtstuff.c (0) 0
  35: OBJECT	LOCAL	__JCR_LIST__ (0) 0x800101fdf0
  36: FUNC	LOCAL	deregister_tm_clones (0) 0x8001020040, rel: 0x20040 (.opd) [0x8001000910, rel: 0x70 (.text)]
  37: FUNC	LOCAL	register_tm_clones (0) 0x8001020050, rel: 0x20050 (.opd) [0x8001000980, rel: 0xe0 (.text)]
  38: FUNC	LOCAL	__do_global_dtors_aux (0) 0x8001020060, rel: 0x20060 (.opd) [0x80010009f0, rel: 0x150 (.text)]
  39: OBJECT	LOCAL	completed.7711 (1) 0x8001020158
  40: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x800101fde8
  41: FUNC	LOCAL	frame_dummy (0) 0x8001020070, rel: 0x20070 (.opd) [0x8001000a50, rel: 0x1b0 (.text)]
  42: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x800101fde0
  43: FILE	LOCAL	foo.c (0) 0
  44: FILE	LOCAL	bar.c (0) 0
  45: OBJECT	LOCAL	b1 (4) 0x8001020004
  46: FUNC	LOCAL	foo (76) 0x8001020090, rel: 0x20090 (.opd) [0x8001000b34, rel: 0x294 (.text)]
  47: FILE	LOCAL	crtstuff.c (0) 0
  48: OBJECT	LOCAL	__FRAME_END__ (0) 0x8001000e18
  49: OBJECT	LOCAL	__JCR_END__ (0) 0x800101fdf0
  50: FILE	LOCAL	 (0) 0
  51: NOTYPE	LOCAL	__glink_PLTresolve (0) 0x8001000ce8
  52: NOTYPE	LOCAL	00000011.plt_call.__libc_start_main@@GLIBC_2.3 (0) 0x80010008a0
  53: NOTYPE	LOCAL	00000011.plt_call.__cxa_finalize@@GLIBC_2.3 (0) 0x80010008b4
  54: NOTYPE	LOCAL	__init_array_end (0) 0x800101fde8
  55: OBJECT	LOCAL	_DYNAMIC (0) 0x800101fe20
  56: NOTYPE	LOCAL	__init_array_start (0) 0x800101fde0
  57: FUNC	GLOBAL	__libc_csu_fini (16) 0x80010200c0, rel: 0x200c0 (.opd) [0x8001000cd0, rel: 0x430 (.text)]
  58: FUNC	GLOBAL	__libc_start_main@@GLIBC_2.3 (0) 0
  59: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  60: NOTYPE	WEAK	data_start (0) 0x8001020000
  61: NOTYPE	GLOBAL	_edata (0) 0x8001020110
  62: FUNC	GLOBAL	bar (116) 0x80010200a0, rel: 0x200a0 (.opd) [0x8001000b80, rel: 0x2e0 (.text)]
  63: FUNC	GLOBAL	_fini (0) 0x8001020030, rel: 0x20030 (.opd) [0x8001000d30, rel: 0 (.fini)]
  64: NOTYPE	GLOBAL	__data_start (0) 0x8001020000
  65: NOTYPE	WEAK	__gmon_start__ (0) 0
  66: OBJECT	GLOBAL	__dso_handle (0) 0x800101fe18
  67: OBJECT	GLOBAL	_IO_stdin_used (4) 0x8001000d4c
  68: OBJECT	GLOBAL	b2 (4) 0x8001020008
  69: FUNC	WEAK	__cxa_finalize@@GLIBC_2.3 (0) 0
  70: FUNC	GLOBAL	__libc_csu_init (204) 0x80010200b0, rel: 0x200b0 (.opd) [0x8001000c00, rel: 0x360 (.text)]
  71: NOTYPE	GLOBAL	_end (0) 0x8001020160
  72: FUNC	GLOBAL	_start (60) 0x8001020010, rel: 0x20010 (.opd) [0x80010008c8, rel: 0x28 (.text)]
  73: NOTYPE	GLOBAL	__bss_start (0) 0x8001020110
  74: FUNC	GLOBAL	main (128) 0x8001020080, rel: 0x20080 (.opd) [0x8001000ab4, rel: 0x214 (.text)]
  75: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  76: OBJECT	GLOBAL	__TMC_END__ (0) 0x8001020010
  77: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  78: FUNC	GLOBAL	_init (0) 0x8001020020, rel: 0x20020 (.opd) [0x8001000850, rel: 0 (.init)]
EOF

cat > testfile.dynsym.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x238
   2: SECTION	LOCAL	 (0) 0x1fdf0
   3: FUNC	GLOBAL	__libc_start_main (0) 0
   4: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
   5: NOTYPE	WEAK	__gmon_start__ (0) 0
   6: FUNC	WEAK	__cxa_finalize (0) 0
   7: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
   8: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
   9: NOTYPE	GLOBAL	_edata (0) 0x20110
  10: NOTYPE	GLOBAL	_end (0) 0x20160
  11: NOTYPE	GLOBAL	__bss_start (0) 0x20110
EOF

cat > testfile.minsym.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x238
   2: SECTION	LOCAL	 (0) 0x1fdf0
   3: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x1fde8
   4: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x1fde0
   5: NOTYPE	LOCAL	__glink_PLTresolve (0) 0xce8
   6: NOTYPE	LOCAL	00000011.plt_call.__libc_start_main@@GLIBC_2.3 (0) 0x8a0
   7: NOTYPE	LOCAL	00000011.plt_call.__cxa_finalize@@GLIBC_2.3 (0) 0x8b4
   8: NOTYPE	LOCAL	__init_array_end (0) 0x1fde8
   9: NOTYPE	LOCAL	__init_array_start (0) 0x1fde0
  10: SECTION	LOCAL	 (0) 0x238
  11: SECTION	LOCAL	 (0) 0x24c
  12: SECTION	LOCAL	 (0) 0x26c
  13: SECTION	LOCAL	 (0) 0x290
  14: SECTION	LOCAL	 (0) 0x2c0
  15: SECTION	LOCAL	 (0) 0x3e0
  16: SECTION	LOCAL	 (0) 0x488
  17: SECTION	LOCAL	 (0) 0x4a0
  18: SECTION	LOCAL	 (0) 0x4c0
  19: SECTION	LOCAL	 (0) 0x820
  20: SECTION	LOCAL	 (0) 0x850
  21: SECTION	LOCAL	 (0) 0x8a0
  22: SECTION	LOCAL	 (0) 0xd30
  23: SECTION	LOCAL	 (0) 0xd4c
  24: SECTION	LOCAL	 (0) 0xd50
  25: SECTION	LOCAL	 (0) 0xd70
  26: SECTION	LOCAL	 (0) 0x1fde0
  27: SECTION	LOCAL	 (0) 0x1fde8
  28: SECTION	LOCAL	 (0) 0x1fdf0
  29: SECTION	LOCAL	 (0) 0x1fdf8
  30: SECTION	LOCAL	 (0) 0x1fe20
  31: SECTION	LOCAL	 (0) 0x20000
  32: SECTION	LOCAL	 (0) 0x20010
  33: SECTION	LOCAL	 (0) 0x200d8
  34: SECTION	LOCAL	 (0) 0x20110
  35: SECTION	LOCAL	 (0) 0x20158
  36: FUNC	GLOBAL	__libc_start_main (0) 0
  37: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  38: NOTYPE	WEAK	__gmon_start__ (0) 0
  39: FUNC	WEAK	__cxa_finalize (0) 0
  40: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  41: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  42: NOTYPE	GLOBAL	_edata (0) 0x20110
  43: NOTYPE	GLOBAL	_end (0) 0x20160
  44: NOTYPE	GLOBAL	__bss_start (0) 0x20110
EOF

cat > testfile.minsym_pl.in <<\EOF
   0: NOTYPE	LOCAL	 (0) 0
   1: SECTION	LOCAL	 (0) 0x8001000238
   2: SECTION	LOCAL	 (0) 0x800101fdf0
   3: OBJECT	LOCAL	__do_global_dtors_aux_fini_array_entry (0) 0x800101fde8
   4: OBJECT	LOCAL	__frame_dummy_init_array_entry (0) 0x800101fde0
   5: NOTYPE	LOCAL	__glink_PLTresolve (0) 0x8001000ce8
   6: NOTYPE	LOCAL	00000011.plt_call.__libc_start_main@@GLIBC_2.3 (0) 0x80010008a0
   7: NOTYPE	LOCAL	00000011.plt_call.__cxa_finalize@@GLIBC_2.3 (0) 0x80010008b4
   8: NOTYPE	LOCAL	__init_array_end (0) 0x800101fde8
   9: NOTYPE	LOCAL	__init_array_start (0) 0x800101fde0
  10: SECTION	LOCAL	 (0) 0x8001000238
  11: SECTION	LOCAL	 (0) 0x800100024c
  12: SECTION	LOCAL	 (0) 0x800100026c
  13: SECTION	LOCAL	 (0) 0x8001000290
  14: SECTION	LOCAL	 (0) 0x80010002c0
  15: SECTION	LOCAL	 (0) 0x80010003e0
  16: SECTION	LOCAL	 (0) 0x8001000488
  17: SECTION	LOCAL	 (0) 0x80010004a0
  18: SECTION	LOCAL	 (0) 0x80010004c0
  19: SECTION	LOCAL	 (0) 0x8001000820
  20: SECTION	LOCAL	 (0) 0x8001000850
  21: SECTION	LOCAL	 (0) 0x80010008a0
  22: SECTION	LOCAL	 (0) 0x8001000d30
  23: SECTION	LOCAL	 (0) 0x8001000d4c
  24: SECTION	LOCAL	 (0) 0x8001000d50
  25: SECTION	LOCAL	 (0) 0x8001000d70
  26: SECTION	LOCAL	 (0) 0x800101fde0
  27: SECTION	LOCAL	 (0) 0x800101fde8
  28: SECTION	LOCAL	 (0) 0x800101fdf0
  29: SECTION	LOCAL	 (0) 0x800101fdf8
  30: SECTION	LOCAL	 (0) 0x800101fe20
  31: SECTION	LOCAL	 (0) 0x8001020000
  32: SECTION	LOCAL	 (0) 0x8001020010
  33: SECTION	LOCAL	 (0) 0x80010200d8
  34: SECTION	LOCAL	 (0) 0x8001020110
  35: SECTION	LOCAL	 (0) 0x8001020158
  36: FUNC	GLOBAL	__libc_start_main (0) 0
  37: NOTYPE	WEAK	_ITM_deregisterTMCloneTable (0) 0
  38: NOTYPE	WEAK	__gmon_start__ (0) 0
  39: FUNC	WEAK	__cxa_finalize (0) 0
  40: NOTYPE	WEAK	_Jv_RegisterClasses (0) 0
  41: NOTYPE	WEAK	_ITM_registerTMCloneTable (0) 0
  42: NOTYPE	GLOBAL	_edata (0) 0x8001020110
  43: NOTYPE	GLOBAL	_end (0) 0x8001020160
  44: NOTYPE	GLOBAL	__bss_start (0) 0x8001020110
EOF

cat testfile.symtab.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebaztabppc64

cat testfile.symtab.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdbgppc64

cat testfile.symtab_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdbgppc64_pl

sed s/0x8001/0x4200/g testfile.symtab_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdbgppc64_plr

cat testfile.dynsym.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazdynppc64

cat testfile.symtab.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazmdbppc64

cat testfile.minsym.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazminppc64

cat testfile.minsym_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazminppc64_pl

sed s/0x8001/0x4200/g testfile.minsym_pl.in \
  | testrun_compare ${abs_builddir}/dwflsyms -e testfilebazminppc64_plr

exit 0
