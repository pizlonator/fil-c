# Copyright (C) 2018 Red Hat, Inc.
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

# - testfile-gnu-property-note.c
# int
# main ()
# {
#   return 0;
# }
#
# gcc -fcf-protection -c testfile-gnu-property-note.c
# gcc -o testfile-gnu-property-note testfile-gnu-property-note.o
# eu-strip --strip-sections testfile-gnu-property-note

testfiles testfile-gnu-property-note.o testfile-gnu-property-note

# Test reading notes through sections
testrun_compare ${abs_top_builddir}/src/readelf -n testfile-gnu-property-note.o << EOF

Note section [ 6] '.note.gnu.property' of 32 bytes at offset 0x80:
  Owner          Data size  Type
  GNU                   16  GNU_PROPERTY_TYPE_0
    X86 FEATURE_1_AND: 00000003 IBT SHSTK
EOF

# Test reading notes through segments
testrun_compare ${abs_top_builddir}/src/readelf -n testfile-gnu-property-note << EOF

Note segment of 32 bytes at offset 0x300:
  Owner          Data size  Type
  GNU                   16  GNU_PROPERTY_TYPE_0
    X86 FEATURE_1_AND: 00000003 IBT SHSTK

Note segment of 68 bytes at offset 0x320:
  Owner          Data size  Type
  GNU                   16  GNU_ABI_TAG
    OS: Linux, ABI: 3.2.0
  GNU                   20  GNU_BUILD_ID
    Build ID: 83cb2229fabd2065d1361f5b46424cd75270f94b
EOF

# NT_VERSION note type clashes with "GNU" owner type NT_GNU_ABI_TAG.
# Uses owner name (with zero desc) for version string.
testfiles testfile11
testrun_compare ${abs_top_builddir}/src/readelf -n testfile11 << EOF

Note section [ 2] '.note.ABI-tag' of 32 bytes at offset 0x128:
  Owner          Data size  Type
  GNU                   16  GNU_ABI_TAG
    OS: Linux, ABI: 2.2.5

Note section [35] '.note' of 60 bytes at offset 0x13364:
  Owner          Data size  Type
  01.01                  0  VERSION
  01.01                  0  VERSION
  01.01                  0  VERSION
EOF

# See run-annobingroup.sh
testfiles testfile-annobingroup.o
testrun_compare ${abs_top_builddir}/src/readelf -n testfile-annobingroup.o << EOF

Note section [ 5] '.gnu.build.attributes' of 272 bytes at offset 0x50:
  Owner          Data size  Type
  GA                    16  GNU Build Attribute OPEN
    Address Range: 0 - 0
    VERSION: "3p8"
  GA                     0  GNU Build Attribute OPEN
    TOOL: "gcc 8.1.1 20180712"
  GA                     0  GNU Build Attribute OPEN
    "GOW": 5
  GA                     0  GNU Build Attribute OPEN
    STACK_PROT: 0
  GA                     0  GNU Build Attribute OPEN
    "stack_clash": FALSE
  GA                     0  GNU Build Attribute OPEN
    "cf_protection": 0
  GA                     0  GNU Build Attribute OPEN
    PIC: 0
  GA                     0  GNU Build Attribute OPEN
    SHORT_ENUM: FALSE
  GA                     0  GNU Build Attribute OPEN
    ABI: c001100000012
  GA                     0  GNU Build Attribute OPEN
    "stack_realign": FALSE

Note section [ 7] '.gnu.build.attributes..text.unlikely' of 216 bytes at offset 0x160:
  Owner          Data size  Type
  GA                    16  GNU Build Attribute FUNC
    Address Range: 0 - 0
    ABI: c001100000012
  GA                     0  GNU Build Attribute FUNC
    "stack_realign": FALSE
  GA                     0  GNU Build Attribute FUNC
    STACK_PROT: 0
  GA                     0  GNU Build Attribute FUNC
    "stack_clash": FALSE
  GA                     0  GNU Build Attribute FUNC
    "cf_protection": 0
  GA                     0  GNU Build Attribute FUNC
    PIC: 0
  GA                     0  GNU Build Attribute FUNC
    "GOW": 5
  GA                     0  GNU Build Attribute FUNC
    SHORT_ENUM: FALSE

Note section [22] '.note.gnu.property' of 48 bytes at offset 0x40c:
  Owner          Data size  Type
  GNU                   32  GNU_PROPERTY_TYPE_0
    X86 0xc0000000 data: 00 00 00 00
    X86 0xc0000001 data: 00 00 00 00
EOF

#
# = gnu_props.S
#
# #define NT_GNU_PROPERTY_TYPE_0 5
# #define GNU_PROPERTY_STACK_SIZE 1
# #define GNU_PROPERTY_NO_COPY_ON_PROTECTED 2

# /* Normal notes always have alignment and padding of 4 bytes,
#    but GNU Property notes use 4 byte words, with 8 byte padding
#    for ELFCLASS64.  */
# #if __SIZEOF_PTRDIFF_T__  == 8
# # define ALIGN 3
# #elif __SIZEOF_PTRDIFF_T__  == 4
# # define ALIGN 2
# #endif
#
# 	.section ".note.gnu.property", "a"
# 	.p2align ALIGN
# 	/* First note.  */
# 	.long 1f - 0f			/* name length.  */
# 	.long 4f - 2f			/* data length.  */
# 	.long NT_GNU_PROPERTY_TYPE_0	/* note type.  */
# 0:
# 	.asciz "GNU"			/* vendor name.  */
# 1:
# 	.p2align ALIGN			/* Padding.  */
# 2:
# 	.long GNU_PROPERTY_STACK_SIZE	/* pr_type.  */
# 	.long 4f - 3f			/* pr_datasz.  */
# 3:
# 	.dc.a 0x280000			/* Stack size.  */
# 4:
# 	.p2align ALIGN
#
# 	/* Second note.  */
# 	.long 6f - 5f				/* name length.  */
# 	.long 8f - 7f				/* data length.  */
# 	.long NT_GNU_PROPERTY_TYPE_0		/* note type.  */
# 5:
# 	.asciz "GNU"				/* vendor name.  */
# 6:
# 	.p2align ALIGN				/* Padding.  */
# 7:
# 	.long GNU_PROPERTY_NO_COPY_ON_PROTECTED /* pr_type.  */
# 	.long 0					/* pr_datasz.  */
# 	/* No data.  */
# 8:
# 	.p2align ALIGN
#
# On x86_64
# gcc -m64 -c -o testfile_gnu_props_64le.o gnu_props.S
# gcc -m32 -c -o testfile_gnu_props_32le.o gnu_props.S

testfiles testfile_gnu_props.32le.o testfile_gnu_props.64le.o

testrun_compare ${abs_top_builddir}/src/readelf -n testfile_gnu_props.32le.o << EOF

Note section [ 4] '.note.gnu.property' of 52 bytes at offset 0x34:
  Owner          Data size  Type
  GNU                   12  GNU_PROPERTY_TYPE_0
    STACK_SIZE 0x280000
  GNU                    8  GNU_PROPERTY_TYPE_0
    NO_COPY_ON_PROTECTION
EOF

testrun_compare ${abs_top_builddir}/src/readelf -n testfile_gnu_props.64le.o << EOF

Note section [ 4] '.note.gnu.property' of 56 bytes at offset 0x40:
  Owner          Data size  Type
  GNU                   16  GNU_PROPERTY_TYPE_0
    STACK_SIZE 0x280000
  GNU                    8  GNU_PROPERTY_TYPE_0
    NO_COPY_ON_PROTECTION
EOF

# On ppc64
# gcc -m32 -c -o testfile_gnu_props.32be.o gnu_props.S
# gcc -m64 -c -o testfile_gnu_props.64be.o gnu_props.S

testfiles testfile_gnu_props.32be.o testfile_gnu_props.64be.o

testrun_compare ${abs_top_builddir}/src/readelf -n testfile_gnu_props.32be.o << EOF

Note section [ 4] '.note.gnu.property' of 52 bytes at offset 0x34:
  Owner          Data size  Type
  GNU                   12  GNU_PROPERTY_TYPE_0
    STACK_SIZE 0x280000
  GNU                    8  GNU_PROPERTY_TYPE_0
    NO_COPY_ON_PROTECTION
EOF

testrun_compare ${abs_top_builddir}/src/readelf -n testfile_gnu_props.64be.o << EOF

Note section [ 4] '.note.gnu.property' of 56 bytes at offset 0x40:
  Owner          Data size  Type
  GNU                   16  GNU_PROPERTY_TYPE_0
    STACK_SIZE 0x280000
  GNU                    8  GNU_PROPERTY_TYPE_0
    NO_COPY_ON_PROTECTION
EOF

# - testfile-gnu-property-note.c
# int
# main ()
# {
#   return 0;
# }
#
# gcc -mbranch-protection=standard -c testfile-gnu-property-note.c
# gcc -o testfile-gnu-property-note-aarch64 testfile-gnu-property-note.o
# eu-strip --remove-section=.gnu.build.attributes \
#	   testfile-gnu-property-note-aarch64

testfiles testfile-gnu-property-note-aarch64
testrun_compare ${abs_top_builddir}/src/readelf -n testfile-gnu-property-note-aarch64 << EOF

Note section [ 2] '.note.gnu.property' of 32 bytes at offset 0x2c8:
  Owner          Data size  Type
  GNU                   16  GNU_PROPERTY_TYPE_0
    AARCH64 FEATURE_1_AND: 00000003 BTI PAC

Note section [ 3] '.note.gnu.build-id' of 36 bytes at offset 0x2e8:
  Owner          Data size  Type
  GNU                   20  GNU_BUILD_ID
    Build ID: af82d6df6f3b396487e3e27a826ca9cbbbecbe5f

Note section [ 4] '.note.ABI-tag' of 32 bytes at offset 0x30c:
  Owner          Data size  Type
  GNU                   16  GNU_ABI_TAG
    OS: Linux, ABI: 3.7.0
EOF
