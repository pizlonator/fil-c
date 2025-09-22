#! /bin/sh
# Copyright (C) 2005, 2006, 2008 Red Hat, Inc.
# This file is part of elfutils.
# Written by Ulrich Drepper <drepper@redhat.com>, 2005.
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

files="testfile `seq 2 9 | while read n; do echo testfile$n; done`"
testfiles $files

testrun_compare ${abs_top_builddir}/src/strings -tx -f $files <<\EOF
testfile:      f4 /lib/ld-linux.so.2
testfile:     1c9 __gmon_start__
testfile:     1d8 libc.so.6
testfile:     1e2 __cxa_finalize
testfile:     1f1 __deregister_frame_info
testfile:     209 _IO_stdin_used
testfile:     218 __libc_start_main
testfile:     22a __register_frame_info
testfile:     240 GLIBC_2.1.3
testfile:     24c GLIBC_2.0
testfile:     338 PTRh
testfile:     345 QVh,
testfile2:     114 /lib/ld.so.1
testfile2:     1f1 __gmon_start__
testfile2:     200 __deregister_frame_info
testfile2:     218 __register_frame_info
testfile2:     22e libc.so.6
testfile2:     238 __cxa_finalize
testfile2:     247 _IO_stdin_used
testfile2:     256 __libc_start_main
testfile2:     268 GLIBC_2.1.3
testfile2:     274 GLIBC_2.0
testfile2:     488 }a[xN
testfile2:     4a8 }a[xN
testfile2:     50c }a[xN
testfile2:     540 }?Kx
testfile3:      f4 /lib/ld-linux.so.2
testfile3:     1c9 __gmon_start__
testfile3:     1d8 libc.so.6
testfile3:     1e2 __cxa_finalize
testfile3:     1f1 __deregister_frame_info
testfile3:     209 _IO_stdin_used
testfile3:     218 __libc_start_main
testfile3:     22a __register_frame_info
testfile3:     240 GLIBC_2.1.3
testfile3:     24c GLIBC_2.0
testfile3:     338 PTRh
testfile3:     345 QVh,
testfile4:      f4 /lib/ld-linux.so.2
testfile4:     8e1 __gmon_start__
testfile4:     8f0 __terminate_func
testfile4:     901 stderr
testfile4:     908 __tf9type_info
testfile4:     917 __tf16__user_type_info
testfile4:     92e __tf19__pointer_type_info
testfile4:     948 __tf16__attr_type_info
testfile4:     95f __tf16__func_type_info
testfile4:     976 __vt_9type_info
testfile4:     986 __vt_19__pointer_type_info
testfile4:     9a1 __vt_16__attr_type_info
testfile4:     9b9 __vt_16__func_type_info
testfile4:     9d1 __vt_16__ptmf_type_info
testfile4:     9e9 __vt_16__ptmd_type_info
testfile4:     a01 __vt_17__array_type_info
testfile4:     a1a __tiv
testfile4:     a20 __vt_19__builtin_type_info
testfile4:     a3b __tix
testfile4:     a41 __til
testfile4:     a47 __tii
testfile4:     a4d __tis
testfile4:     a53 __tib
testfile4:     a59 __tic
testfile4:     a5f __tiw
testfile4:     a65 __tir
testfile4:     a6b __tid
testfile4:     a71 __tif
testfile4:     a77 __tiUi
testfile4:     a7e __tiUl
testfile4:     a85 __tiUx
testfile4:     a8c __tiUs
testfile4:     a93 __tiUc
testfile4:     a9a __tiSc
testfile4:     aa1 __ti19__pointer_type_info
testfile4:     abb __ti9type_info
testfile4:     aca __ti16__attr_type_info
testfile4:     ae1 __ti19__builtin_type_info
testfile4:     afb __ti16__func_type_info
testfile4:     b12 __ti16__ptmf_type_info
testfile4:     b29 __ti16__ptmd_type_info
testfile4:     b40 __ti17__array_type_info
testfile4:     b58 __cplus_type_matcher
testfile4:     b6d __vt_13bad_exception
testfile4:     b82 __vt_9exception
testfile4:     b92 _._13bad_exception
testfile4:     ba5 __vt_8bad_cast
testfile4:     bb4 _._8bad_cast
testfile4:     bc1 __vt_10bad_typeid
testfile4:     bd3 _._10bad_typeid
testfile4:     be3 __ti9exception
testfile4:     bf2 __ti13bad_exception
testfile4:     c06 __vt_16__user_type_info
testfile4:     c1e __vt_17__class_type_info
testfile4:     c37 __vt_14__si_type_info
testfile4:     c4d __ti8bad_cast
testfile4:     c5b __ti10bad_typeid
testfile4:     c6c __ti16__user_type_info
testfile4:     c83 __ti14__si_type_info
testfile4:     c98 __ti17__class_type_info
testfile4:     cb0 libc.so.6
testfile4:     cba __register_frame
testfile4:     ccb pthread_create
testfile4:     cda pthread_getspecific
testfile4:     cee pthread_key_delete
testfile4:     d01 __cxa_finalize
testfile4:     d10 malloc
testfile4:     d17 __frame_state_for
testfile4:     d29 abort
testfile4:     d2f __register_frame_table
testfile4:     d46 fprintf
testfile4:     d4e pthread_once
testfile4:     d5b __deregister_frame_info
testfile4:     d73 pthread_key_create
testfile4:     d86 memset
testfile4:     d8d strcmp
testfile4:     d94 pthread_mutex_unlock
testfile4:     da9 __deregister_frame
testfile4:     dbc pthread_mutex_lock
testfile4:     dcf _IO_stdin_used
testfile4:     dde __libc_start_main
testfile4:     df0 strlen
testfile4:     df7 __register_frame_info_table
testfile4:     e13 __register_frame_info
testfile4:     e29 pthread_setspecific
testfile4:     e3d free
testfile4:     e42 GLIBC_2.1.3
testfile4:     e4e GLIBC_2.0
testfile4:    1308 PTRh<
testfile4:    194b [^_]
testfile4:    19bf [^_]
testfile4:    1dd9 wT9L>
testfile4:    1f3b [^_]
testfile4:    1fae [^_]
testfile4:    21c1 BZQRP
testfile4:    237f [^_]
testfile4:    2431 JWRV
testfile4:    2454 [^_]
testfile4:    2506 JWRV
testfile4:    2529 [^_]
testfile4:    2b6c [^_]
testfile4:    2b9d ZYPV
testfile4:    2c28 [^_]
testfile4:    2c4d ZYPV
testfile4:    2ce2 [^_]
testfile4:    2dfb X^_]
testfile4:    2fc8 [^_]
testfile4:    307d tq;F
testfile4:    315a [^_]
testfile4:    31a5 :zt	1
testfile4:    3238 [^_]
testfile4:    32f8 AXY_VR
testfile4:    334a [^_]
testfile4:    37ab [^_]
testfile4:    38b8 sU;E
testfile4:    38f2 QRPV
testfile4:    3926 [^_]
testfile4:    3bfe QRWP
testfile4:    3e65 [^_]
testfile4:    4136 [^_]
testfile4:    472d [^_]
testfile4:    47a5 0[^_]
testfile4:    48ab [^_]
testfile4:    4ab1 _ZPV
testfile4:    4b53 _ZPV
testfile4:    4bd3 _ZPV
testfile4:    4e05 PQWj
testfile4:    4f75 [^_]
testfile4:    4f9b u$;E u
testfile4:    4feb [^_]
testfile4:    5080 [^_]
testfile4:    50a8 }$9u
testfile4:    5149 [^_]
testfile4:    51b0 [^_]
testfile4:    539b [^_]
testfile4:    53b5 E 9E
testfile4:    540d x!)E 
testfile4:    5598 U$	B
testfile4:    571c [^_]
testfile4:    5819 [^_]
testfile4:    5922 [^_]
testfile4:    59c2 [^_]
testfile4:    5a62 [^_]
testfile4:    5b02 [^_]
testfile4:    5ba2 [^_]
testfile4:    5c42 [^_]
testfile4:    5ce2 [^_]
testfile4:    6112 [^_]
testfile4:    62bb [^_]
testfile4:    639b [^_]
testfile4:    6436 [^_]
testfile4:    6468 val is zero
testfile4:    6480 Internal Compiler Bug: No runtime type matcher.
testfile4:    64dc 19__pointer_type_info
testfile4:    64f2 16__attr_type_info
testfile4:    6505 19__builtin_type_info
testfile4:    651b 16__func_type_info
testfile4:    652e 16__ptmf_type_info
testfile4:    6541 16__ptmd_type_info
testfile4:    6554 17__array_type_info
testfile4:    6568 9exception
testfile4:    6573 13bad_exception
testfile4:    6583 9type_info
testfile4:    658e 8bad_cast
testfile4:    6598 10bad_typeid
testfile4:    65a5 16__user_type_info
testfile4:    65b8 14__si_type_info
testfile4:    65c9 17__class_type_info
testfile4:    6fc1 H. $
testfile5:      f4 /lib/ld-linux.so.2
testfile5:     1c9 __gmon_start__
testfile5:     1d8 libc.so.6
testfile5:     1e2 __cxa_finalize
testfile5:     1f1 __deregister_frame_info
testfile5:     209 _IO_stdin_used
testfile5:     218 __libc_start_main
testfile5:     22a __register_frame_info
testfile5:     240 GLIBC_2.1.3
testfile5:     24c GLIBC_2.0
testfile5:     338 PTRh
testfile5:     345 QVhD
testfile6:     114 /lib/ld-linux.so.2
testfile6:     3d9 libstdc++.so.5
testfile6:     3e8 _ZTVSt16invalid_argument
testfile6:     401 _ZNSaIcEC1Ev
testfile6:     40e _ZTSSt16invalid_argument
testfile6:     427 _ZTVN10__cxxabiv120__si_class_type_infoE
testfile6:     450 _ZNSsD1Ev
testfile6:     45a _ZdlPv
testfile6:     461 __cxa_end_catch
testfile6:     471 __gxx_personality_v0
testfile6:     486 _ZTISt9exception
testfile6:     497 _ZNSaIcED1Ev
testfile6:     4a4 _ZTISt11logic_error
testfile6:     4b8 _ZNSt16invalid_argumentD1Ev
testfile6:     4d4 _ZTVN10__cxxabiv117__class_type_infoE
testfile6:     4fa __cxa_throw
testfile6:     506 _ZNSt16invalid_argumentC1ERKSs
testfile6:     525 _ZNSsC1EPKcRKSaIcE
testfile6:     538 _ZNSt11logic_errorD2Ev
testfile6:     54f _ZTVN10__cxxabiv121__vmi_class_type_infoE
testfile6:     579 _ZNSt16invalid_argumentD0Ev
testfile6:     595 __cxa_begin_catch
testfile6:     5a7 __cxa_allocate_exception
testfile6:     5c0 _ZNKSt11logic_error4whatEv
testfile6:     5db _Jv_RegisterClasses
testfile6:     5ef _ZTISt16invalid_argument
testfile6:     608 __gmon_start__
testfile6:     617 libm.so.6
testfile6:     621 _IO_stdin_used
testfile6:     630 libgcc_s.so.1
testfile6:     63e _Unwind_Resume
testfile6:     64d libc.so.6
testfile6:     657 __libc_start_main
testfile6:     669 GCC_3.0
testfile6:     671 GLIBC_2.0
testfile6:     67b GLIBCPP_3.2
testfile6:     687 CXXABI_1.2
testfile6:     908 PTRh 
testfile6:     e48 gdb.1
testfile6:     ec8 N10__gnu_test9gnu_obj_1E
testfile6:     ee1 N10__gnu_test9gnu_obj_2IiEE
testfile6:     efd N10__gnu_test9gnu_obj_2IlEE
testfile6:     f19 St16invalid_argument
testfile7:     114 /lib/ld-linux.so.2
testfile7:     3d9 libstdc++.so.5
testfile7:     3e8 _ZTVSt16invalid_argument
testfile7:     401 _ZNSaIcEC1Ev
testfile7:     40e _ZTSSt16invalid_argument
testfile7:     427 _ZTVN10__cxxabiv120__si_class_type_infoE
testfile7:     450 _ZNSsD1Ev
testfile7:     45a _ZdlPv
testfile7:     461 __cxa_end_catch
testfile7:     471 __gxx_personality_v0
testfile7:     486 _ZTISt9exception
testfile7:     497 _ZNSaIcED1Ev
testfile7:     4a4 _ZTISt11logic_error
testfile7:     4b8 _ZNSt16invalid_argumentD1Ev
testfile7:     4d4 _ZTVN10__cxxabiv117__class_type_infoE
testfile7:     4fa __cxa_throw
testfile7:     506 _ZNSt16invalid_argumentC1ERKSs
testfile7:     525 _ZNSsC1EPKcRKSaIcE
testfile7:     538 _ZNSt11logic_errorD2Ev
testfile7:     54f _ZTVN10__cxxabiv121__vmi_class_type_infoE
testfile7:     579 _ZNSt16invalid_argumentD0Ev
testfile7:     595 __cxa_begin_catch
testfile7:     5a7 __cxa_allocate_exception
testfile7:     5c0 _ZNKSt11logic_error4whatEv
testfile7:     5db _Jv_RegisterClasses
testfile7:     5ef _ZTISt16invalid_argument
testfile7:     608 __gmon_start__
testfile7:     617 libm.so.6
testfile7:     621 _IO_stdin_used
testfile7:     630 libgcc_s.so.1
testfile7:     63e _Unwind_Resume
testfile7:     64d libc.so.6
testfile7:     657 __libc_start_main
testfile7:     669 GCC_3.0
testfile7:     671 GLIBC_2.0
testfile7:     67b GLIBCPP_3.2
testfile7:     687 CXXABI_1.2
testfile7:     908 PTRh 
testfile7:     e48 gdb.1
testfile7:     ec8 N10__gnu_test9gnu_obj_1E
testfile7:     ee1 N10__gnu_test9gnu_obj_2IiEE
testfile7:     efd N10__gnu_test9gnu_obj_2IlEE
testfile7:     f19 St16invalid_argument
testfile8:      79 XZh;
testfile8:      87 YXh<
testfile8:     14f SQh[
testfile8:     259 t5Wj
testfile8:     502 WRVQ
testfile8:    1fe7 ZYPj
testfile8:    2115 u'Pj
testfile8:    7bba FILE
testfile8:    7bbf preserve-dates
testfile8:    7bce remove-comment
testfile8:    7bdd Remove .comment section
testfile8:    7bf6 ${prefix}/share
testfile8:    7c06 elfutils
testfile8:    7c0f a.out
testfile8:    7c15 0.58
testfile8:    7c1a strip (Red Hat %s) %s
testfile8:    7c31 2002
testfile8:    7c36 Ulrich Drepper
testfile8:    7c45 Written by %s.
testfile8:    7c55 cannot stat input file "%s"
testfile8:    7c71 %s: INTERNAL ERROR: %s
testfile8:    7c88 while opening "%s"
testfile8:    7c9b handle_elf
testfile8:    7ca6 ../../src/strip.c
testfile8:    7cb8 shdr_info[cnt].group_idx != 0
testfile8:    7cd6 illformed file `%s'
testfile8:    7cea elf_ndxscn (scn) == cnt
testfile8:    7d02 .shstrtab
testfile8:    7d0c while writing `%s': %s
testfile8:    7d23 ((sym->st_info) & 0xf) == 3
testfile8:    7d3f shndxdata != ((void *)0)
testfile8:    7d58 scn != ((void *)0)
testfile8:    7d6b .gnu_debuglink
testfile8:    7d7a .comment
testfile8:    7d83 cannot open `%s'
testfile8:    7da0 Place stripped output into FILE
testfile8:    7dc0 Extract the removed sections into FILE
testfile8:    7e00 Copy modified/access timestamps to the output
testfile8:    7e40 Only one input file allowed together with '-o' and '-f'
testfile8:    7e80 Copyright (C) %s Red Hat, Inc.
testfile8:    7e9f This is free software; see the source for copying conditions.  There is NO
testfile8:    7eea warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
testfile8:    7f40 Report bugs to <drepper@redhat.com>.
testfile8:    7f80 %s: File format not recognized
testfile8:    7fa0 cannot set access and modification date of "%s"
testfile8:    7fe0 cannot create new file `%s': %s
testfile8:    8000 error while finishing `%s': %s
testfile8:    8020 shdr_info[shdr_info[cnt].shdr.sh_link].version_idx == 0
testfile8:    8060 shdr_info[shdr_info[cnt].shdr.sh_link].symtab_idx == 0
testfile8:    80a0 %s: error while creating ELF header: %s
testfile8:    80e0 %s: error while reading the file: %s
testfile8:    8120 sec < 0xff00 || shndxdata != ((void *)0)
testfile8:    8160 (versiondata->d_size / sizeof (GElf_Versym)) >= shdr_info[cnt].data->d_size / elsize
testfile8:    81c0 shdr_info[cnt].shdr.sh_type == 11
testfile8:    8200 (versiondata->d_size / sizeof (Elf32_Word)) >= shdr_info[cnt].data->d_size / elsize
testfile8:    8260 shdr_info[cnt].shdr.sh_type == 18
testfile8:    82a0 shdr_info[cnt].data != ((void *)0)
testfile8:    82e0 elf_ndxscn (shdr_info[cnt].newscn) == idx
testfile8:    8320 while create section header section: %s
testfile8:    8360 cannot allocate section data: %s
testfile8:    83a0 elf_ndxscn (shdr_info[cnt].newscn) == shdr_info[cnt].idx
testfile8:    83e0 while generating output file: %s
testfile8:    8420 while preparing output for `%s'
testfile8:    8440 shdr_info[cnt].shdr.sh_type == 2
testfile8:    8480 shdr_info[idx].data != ((void *)0)
testfile8:    84c0 cannot determine number of sections: %s
testfile8:    8500 cannot get section header string table index
testfile8:    85c0 Discard symbols from object files.
testfile8:    85e3 [FILE...]
testfile9:      79 XZh;
testfile9:      87 YXh<
testfile9:     14f SQh[
testfile9:     259 t5Wj
testfile9:     502 WRVQ
testfile9:    1fe7 ZYPj
testfile9:    2115 u'Pj
testfile9:    3414 FILE
testfile9:    3419 preserve-dates
testfile9:    3428 remove-comment
testfile9:    3437 Remove .comment section
testfile9:    3450 ${prefix}/share
testfile9:    3460 elfutils
testfile9:    3469 a.out
testfile9:    346f 0.58
testfile9:    3474 strip (Red Hat %s) %s
testfile9:    348b 2002
testfile9:    3490 Ulrich Drepper
testfile9:    349f Written by %s.
testfile9:    34af cannot stat input file "%s"
testfile9:    34cb %s: INTERNAL ERROR: %s
testfile9:    34e2 while opening "%s"
testfile9:    34f5 handle_elf
testfile9:    3500 ../../src/strip.c
testfile9:    3512 shdr_info[cnt].group_idx != 0
testfile9:    3530 illformed file `%s'
testfile9:    3544 elf_ndxscn (scn) == cnt
testfile9:    355c .shstrtab
testfile9:    3566 while writing `%s': %s
testfile9:    357d ((sym->st_info) & 0xf) == 3
testfile9:    3599 shndxdata != ((void *)0)
testfile9:    35b2 scn != ((void *)0)
testfile9:    35c5 .gnu_debuglink
testfile9:    35d4 .comment
testfile9:    35dd cannot open `%s'
testfile9:    3600 Place stripped output into FILE
testfile9:    3620 Extract the removed sections into FILE
testfile9:    3660 Copy modified/access timestamps to the output
testfile9:    36a0 Only one input file allowed together with '-o' and '-f'
testfile9:    36e0 Copyright (C) %s Red Hat, Inc.
testfile9:    36ff This is free software; see the source for copying conditions.  There is NO
testfile9:    374a warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
testfile9:    37a0 Report bugs to <drepper@redhat.com>.
testfile9:    37e0 %s: File format not recognized
testfile9:    3800 cannot set access and modification date of "%s"
testfile9:    3840 cannot create new file `%s': %s
testfile9:    3860 error while finishing `%s': %s
testfile9:    3880 shdr_info[shdr_info[cnt].shdr.sh_link].version_idx == 0
testfile9:    38c0 shdr_info[shdr_info[cnt].shdr.sh_link].symtab_idx == 0
testfile9:    3900 %s: error while creating ELF header: %s
testfile9:    3940 %s: error while reading the file: %s
testfile9:    3980 sec < 0xff00 || shndxdata != ((void *)0)
testfile9:    39c0 (versiondata->d_size / sizeof (GElf_Versym)) >= shdr_info[cnt].data->d_size / elsize
testfile9:    3a20 shdr_info[cnt].shdr.sh_type == 11
testfile9:    3a60 (versiondata->d_size / sizeof (Elf32_Word)) >= shdr_info[cnt].data->d_size / elsize
testfile9:    3ac0 shdr_info[cnt].shdr.sh_type == 18
testfile9:    3b00 shdr_info[cnt].data != ((void *)0)
testfile9:    3b40 elf_ndxscn (shdr_info[cnt].newscn) == idx
testfile9:    3b80 while create section header section: %s
testfile9:    3bc0 cannot allocate section data: %s
testfile9:    3c00 elf_ndxscn (shdr_info[cnt].newscn) == shdr_info[cnt].idx
testfile9:    3c40 while generating output file: %s
testfile9:    3c80 while preparing output for `%s'
testfile9:    3ca0 shdr_info[cnt].shdr.sh_type == 2
testfile9:    3ce0 shdr_info[idx].data != ((void *)0)
testfile9:    3d20 cannot determine number of sections: %s
testfile9:    3d60 cannot get section header string table index
testfile9:    3e20 Discard symbols from object files.
testfile9:    3e43 [FILE...]
EOF

exit 0
