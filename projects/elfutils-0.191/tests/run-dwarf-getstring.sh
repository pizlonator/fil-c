#! /bin/sh
# Copyright (C) 2011 Red Hat, Inc.
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

testfiles testfile11

testrun_compare ${abs_builddir}/dwarf-getstring testfile11 <<\EOF
_ZNSbIwSt11char_traitsIwESaIwEE13_S_copy_charsEPwS3_S3_
itimerspec
_G_int32_t
_IO_last_state
antiquities
_ZNSbIwSt11char_traitsIwESaIwEEpLEw
insert
_ZNSbIwSt11char_traitsIwESaIwEE12_M_leak_hardEv
__lockkind
_ZNKSbIwSt11char_traitsIwESaIwEE8capacityEv
_ZNSs7_M_leakEv
_M_ref_count
_ZNSt11char_traitsIwE6assignEPwjw
_ZNKSs13find_first_ofEPKcj
._14
._15
._16
._17
_ZNKSs16find_last_not_ofEPKcj
_G_iconv_t
_ZN10__gnu_test9gnu_obj_2IlEaSERKS1_
_ZN11random_dataaSERKS_
_ZNSt11char_traitsIcE7not_eofERKi
__class_type_info
tm_sec
_ZNKSbIwSt11char_traitsIwESaIwEE5c_strEv
__rlim64_t
seek
pthread_mutex_t
_ZNSs5eraseEN9__gnu_cxx17__normal_iteratorIPcSsEE
_ZNSsaSEc
__not_va_list__
_ZNKSs12find_last_ofEPKcj
_ZNSs7replaceEN9__gnu_cxx17__normal_iteratorIPcSsEES2_S2_S2_
__gconv_info
_ZNSt11__ios_flags12_S_showpointE
output_iterator_tag
gnu_obj_2<long int>
_ZNSs6insertEjRKSsjj
_ZN13__type_traitsIbEaSERKS0_
_ZNKSbIwSt11char_traitsIwESaIwEE4copyEPwjj
_ZNSs9_M_mutateEjjj
__ios_flags
short unsigned int
_ZNKSs4findEPKcj
compare
_ZNSbIwSt11char_traitsIwESaIwEE4_Rep7_M_grabERKS1_S5_
tm_yday
unsigned char
__stacksize
__gconv_init_fct
_IO_FILE
__counter
._26
._27
bidirectional_iterator_tag
._29
it_value
const_iterator
_ZNSt11__ios_flags6_S_outE
_M_set_leaked
_Is_integer<unsigned int>
__value
timeval
_IO_jump_t
_ZN11sched_paramaSERKS_
__normal_iterator<char*,std::basic_string<char, std::char_traits<char>, std::allocator<char> > >
_ZNSs4_Rep7_M_grabERKSaIcES2_
_wide_vtable
__codecvt_destr
_STL_mutex_lock
_ZNSt24__default_alloc_templateILb1ELi0EE17_S_freelist_indexEj
_ZNSbIwSt11char_traitsIwESaIwEE7replaceEjjjw
_ZN17__gconv_step_dataaSERKS_
__w_stopval
__int64_t
__type_traits<double>
~_Lock
_ZNKSbIwSt11char_traitsIwESaIwEE5beginEv
ptrdiff_t
test
_Integral
cookie_seek_function_t
__vmi_class_type_info
_ZNSs7replaceEjjjc
__int32_t
register_t
~_STL_auto_lock
_ZNKSbIwSt11char_traitsIwESaIwEE16find_last_not_ofEPKwjj
__arg
_ZNSs7replaceEjjPKcj
_ZNSbIwSt11char_traitsIwESaIwEE7replaceEjjRKS2_jj
_ZNKSbIwSt11char_traitsIwESaIwEE12find_last_ofEPKwjj
_ZN11_Is_integerImEaSERKS0_
__default_alloc_template
_S_hex
__statep
_ZNSt11char_traitsIwE2ltERKwS2_
_M_p
_ZNKSs4sizeEv
EOF

exit 0
