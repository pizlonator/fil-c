#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

# A random 32bit and 64bit testfile
testfiles testfile testfile10

testrun_compare ${abs_top_builddir}/tests/zstrptr testfile <<\EOF
Strings in section 32 (compressed):
[0] ''
[1] '.symtab'
[9] '.strtab'
[11] '.shstrtab'
[1b] '.interp'
[23] '.note.ABI-tag'
[31] '.hash'
[37] '.dynsym'
[3f] '.dynstr'
[47] '.gnu.version'
[54] '.gnu.version_r'
[63] '.rel.got'
[6c] '.rel.plt'
[75] '.init'
[7b] '.plt'
[80] '.text'
[86] '.fini'
[8c] '.rodata'
[94] '.data'
[9a] '.eh_frame'
[a4] '.ctors'
[ab] '.dtors'
[b2] '.got'
[b7] '.dynamic'
[c0] '.sbss'
[c6] '.bss'
[cb] '.stab'
[d1] '.stabstr'
[da] '.comment'
[e3] '.debug_aranges'
[f2] '.debug_pubnames'
[102] '.debug_info'
[10e] '.debug_abbrev'
[11c] '.debug_line'
[128] '.note'
Strings in section 32 (uncompressed):
[0] ''
[1] '.symtab'
[9] '.strtab'
[11] '.shstrtab'
[1b] '.interp'
[23] '.note.ABI-tag'
[31] '.hash'
[37] '.dynsym'
[3f] '.dynstr'
[47] '.gnu.version'
[54] '.gnu.version_r'
[63] '.rel.got'
[6c] '.rel.plt'
[75] '.init'
[7b] '.plt'
[80] '.text'
[86] '.fini'
[8c] '.rodata'
[94] '.data'
[9a] '.eh_frame'
[a4] '.ctors'
[ab] '.dtors'
[b2] '.got'
[b7] '.dynamic'
[c0] '.sbss'
[c6] '.bss'
[cb] '.stab'
[d1] '.stabstr'
[da] '.comment'
[e3] '.debug_aranges'
[f2] '.debug_pubnames'
[102] '.debug_info'
[10e] '.debug_abbrev'
[11c] '.debug_line'
[128] '.note'
EOF

testrun_compare ${abs_top_builddir}/tests/zstrptr testfile10 <<\EOF
Strings in section 30 (compressed):
[0] ''
[1] '.symtab'
[9] '.strtab'
[11] '.shstrtab'
[1b] '.hash'
[21] '.dynsym'
[29] '.dynstr'
[31] '.gnu.version'
[3e] '.gnu.version_r'
[4d] '.rela.dyn'
[57] '.init'
[5d] '.text'
[63] '.fini'
[69] '.eh_frame'
[73] '.data'
[79] '.dynamic'
[82] '.ctors'
[89] '.dtors'
[90] '.jcr'
[95] '.plt'
[9a] '.got'
[9f] '.sdata'
[a6] '.sbss'
[ac] '.bss'
[b1] '.comment'
[ba] '.debug_aranges'
[c9] '.debug_pubnames'
[d9] '.debug_abbrev'
[e7] '.debug_line'
[f3] '.debug_frame'
[100] '.debug_str'
[10b] '.rela.debug_info'
Strings in section 30 (uncompressed):
[0] ''
[1] '.symtab'
[9] '.strtab'
[11] '.shstrtab'
[1b] '.hash'
[21] '.dynsym'
[29] '.dynstr'
[31] '.gnu.version'
[3e] '.gnu.version_r'
[4d] '.rela.dyn'
[57] '.init'
[5d] '.text'
[63] '.fini'
[69] '.eh_frame'
[73] '.data'
[79] '.dynamic'
[82] '.ctors'
[89] '.dtors'
[90] '.jcr'
[95] '.plt'
[9a] '.got'
[9f] '.sdata'
[a6] '.sbss'
[ac] '.bss'
[b1] '.comment'
[ba] '.debug_aranges'
[c9] '.debug_pubnames'
[d9] '.debug_abbrev'
[e7] '.debug_line'
[f3] '.debug_frame'
[100] '.debug_str'
[10b] '.rela.debug_info'
EOF

exit 0
