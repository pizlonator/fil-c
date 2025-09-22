. $srcdir/test-subr.sh

# - s.c
# int main_argc_remaining;
#
# int main_argc() {
#   int result = 0;
#   if (main_argc_remaining)
#     result = 0;
#
#   return 0;
# }
#
# gcc -gdwarf-5 -c -o testfile-dwarf5-fat-lto.o -flto -O s.c -g -ffat-lto-objects

testfiles testfile-dwarf5-fat-lto.o
testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=loc --debug-dump=ranges -N -U testfile-dwarf5-fat-lto.o << EOF

DWARF section [26] '.debug_loclists' at offset 0x7db:
Table at Offset 0x0:

 Length:               24
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        0
 CU [     c] base: 000000000000000000

  Offset: c, Index: 0
    view pair 2, 3

  Offset: e, Index: 2
    start_length 0x0, 0
        [ 0] lit0
        [ 1] stack_value
    end_of_list


DWARF section [30] '.debug_rnglists' at offset 0x827:
Table at Offset 0x0:

 Length:               19
 DWARF version:         5
 Address size:          8
 Segment size:          0
 Offset entries:        0
 CU [     c] base: 000000000000000000

  Offset: c, Index: 0
    start_length 0x0, 8
    end_of_list

EOF
