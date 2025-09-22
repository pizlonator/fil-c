#! /bin/sh
# Copyright (C) 2015, 2018 Red Hat, Inc.
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

testfiles debug-ranges-no-lowpc.o

testrun_compare ${abs_builddir}/dwarf-ranges debug-ranges-no-lowpc.o 0xb <<\EOF
1..2 (base 0)
3..4 (base 0)
EOF

# - hello.c
# int say (const char *prefix);
#
# char *
# subject (char *word, int count)
# {
#    return count > 0 ? word : (word + count);
# }
#
# int
# main (int argc, char **argv)
# {
#    return say (subject (argv[0], argc));
# }
#
# int
# no_say (const char *prefix)
# {
#   const char *world = subject ("World", 42);
#   return prefix ? say (prefix) : say (world);
# }
#
# - world.c
# char * subject (char *word, int count);
# int no_say (const char *prefix);
#
# static int
# sad (char c)
# {
#   return c > 0 ? c : c + 1;
# }
#
# static int
# happy (const char *w)
# {
#   return sad (w[1]);
# }
#
# int
# say (const char *prefix)
# {
#   const char *world = subject ("World", 42);;
#   return prefix ? sad (prefix[0]) : happy (world);
# }
#
# char *
# no_subject (char *word, int count)
# {
#    return count > 0 ? word : (word + count);
# }
#
# int
# no_main (int argc, char **argv)
# {
#    return no_say (no_subject (argv[0], argc));
# }
#
# - gcc -c -O2 -gdwarf-4 hello.c
# - gcc -c -O2 -gdwarf-4 world.c
# - gcc -o testfileranges4 -O2 -gdwarf-4 hello.o world.o
# - eu-strip -f testfileranges4.debug testfileranges4

testfiles testfileranges4.debug
testrun_compare ${abs_builddir}/dwarf-ranges testfileranges4.debug 0xb <<\EOF
400500..40053a (base 0)
400400..400415 (base 0)
EOF

testrun_compare ${abs_builddir}/dwarf-ranges testfileranges4.debug 0xcd <<\EOF
400400..400402 (base 0)
400405..40040d (base 0)
EOF

testrun_compare ${abs_builddir}/dwarf-ranges testfileranges4.debug 0x374 <<\EOF
4005a0..4005a2 (base 400540)
4005a5..4005ad (base 400540)
EOF

# Like above, but with -gdwarf-5.
testfiles testfileranges5.debug
testrun_compare ${abs_builddir}/dwarf-ranges testfileranges5.debug 0xc <<\EOF
401150..40117a (base 0)
401050..401067 (base 0)
EOF

testrun_compare ${abs_builddir}/dwarf-ranges testfileranges5.debug 0x2ce <<\EOF
40119b..40119b (base 401180)
40119c..4011a6 (base 401180)
4011b0..4011b4 (base 401180)
4011b5..4011bf (base 401180)
EOF

testrun_compare ${abs_builddir}/dwarf-ranges testfileranges5.debug 0x2ef <<\EOF
40119b..40119b (base 401180)
40119c..4011a6 (base 401180)
4011b4..4011b4 (base 401180)
4011b5..4011bf (base 401180)
EOF

exit 0
