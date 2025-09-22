#! /bin/sh
# Copyright (c) 2024 Meta Platforms, Inc. and affiliates.
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

# see tests/testfile-dwarf-45.source
testfiles testfile-dwarf-4 testfile-dwarf-5
testfiles testfile-splitdwarf-4 testfile-hello4.dwo testfile-world4.dwo
testfiles testfile-splitdwarf-5 testfile-hello5.dwo testfile-world5.dwo

testrun_compare ${abs_builddir}/declfiles testfile-dwarf-4 << EOF
file: testfile-dwarf-4
 cu: hello.c
  wchar_t@/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h:328:24
  m@/var/tmp/hello/hello.h:1:12
  m@/var/tmp/hello/hello.c:5:5
  foo@/var/tmp/hello/hello.c:20:1
   f@/var/tmp/hello/hello.c:20:14
  baz@/var/tmp/hello/hello.c:8:5
   x@/var/tmp/hello/hello.c:8:14
   r@/var/tmp/hello/hello.c:10:7
  frob@/var/tmp/hello/hello.h:5:1
   a@/var/tmp/hello/hello.h:5:11
   b@/var/tmp/hello/hello.h:5:18
   c@/var/tmp/hello/hello.h:7:7
  foo@/var/tmp/hello/hello.c:20:1
   f@/var/tmp/hello/hello.c:20:14
   frob@/var/tmp/hello/hello.h:5:1
    b@/var/tmp/hello/hello.h:5:18
    a@/var/tmp/hello/hello.h:5:11
    c@/var/tmp/hello/hello.h:7:7
    baz@/var/tmp/hello/hello.c:8:5
     x@/var/tmp/hello/hello.c:8:14
     r@/var/tmp/hello/hello.c:10:7
   foo@/var/tmp/hello/hello.c:20:1
    f@/var/tmp/hello/hello.c:20:14
    main@/var/tmp/hello/hello.c:4:12
  baz@/var/tmp/hello/hello.c:8:5
   x@/var/tmp/hello/hello.c:8:14
   r@/var/tmp/hello/hello.c:10:7
   foo@/var/tmp/hello/hello.c:20:1
  main@/var/tmp/hello/hello.c:4:12
 cu: world.c
  m@/var/tmp/hello/hello.h:1:12
  main@/var/tmp/hello/world.c:14:1
   argc@/var/tmp/hello/world.c:14:11
   argv@/var/tmp/hello/world.c:14:30
   n@/var/tmp/hello/world.c:16:15
   calc@/var/tmp/hello/world.c:5:1
   exit@/usr/include/stdlib.h:542:13
  calc@/var/tmp/hello/world.c:5:1
   word@/var/tmp/hello/world.c:5:19
   frob@/var/tmp/hello/hello.h:5:1
    b@/var/tmp/hello/hello.h:5:18
    a@/var/tmp/hello/hello.h:5:11
    c@/var/tmp/hello/hello.h:7:7
    baz@/var/tmp/hello/hello.h:2:12
  frob@/var/tmp/hello/hello.h:5:1
   a@/var/tmp/hello/hello.h:5:11
   b@/var/tmp/hello/hello.h:5:18
   c@/var/tmp/hello/hello.h:7:7
  exit@/usr/include/stdlib.h:542:13
  baz@/var/tmp/hello/hello.h:2:12
EOF

testrun_compare ${abs_builddir}/declfiles testfile-dwarf-5 << EOF
file: testfile-dwarf-5
 cu: hello.c
  wchar_t@/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h:328:24
  m@/var/tmp/hello/hello.h:1:12
  m@/var/tmp/hello/hello.c:5:5
  foo@/var/tmp/hello/hello.c:20:1
   f@/var/tmp/hello/hello.c:20:14
  baz@/var/tmp/hello/hello.c:8:5
   x@/var/tmp/hello/hello.c:8:14
   r@/var/tmp/hello/hello.c:10:7
  frob@/var/tmp/hello/hello.h:5:1
   a@/var/tmp/hello/hello.h:5:11
   b@/var/tmp/hello/hello.h:5:18
   c@/var/tmp/hello/hello.h:7:7
  foo@/var/tmp/hello/hello.c:20:1
   f@/var/tmp/hello/hello.c:20:14
   frob@/var/tmp/hello/hello.h:5:1
    b@/var/tmp/hello/hello.h:5:18
    a@/var/tmp/hello/hello.h:5:11
    c@/var/tmp/hello/hello.h:7:7
    baz@/var/tmp/hello/hello.c:8:5
     x@/var/tmp/hello/hello.c:8:14
     r@/var/tmp/hello/hello.c:10:7
   foo@/var/tmp/hello/hello.c:20:1
    f@/var/tmp/hello/hello.c:20:14
  baz@/var/tmp/hello/hello.c:8:5
   x@/var/tmp/hello/hello.c:8:14
   r@/var/tmp/hello/hello.c:10:7
  main@/var/tmp/hello/hello.c:4:12
 cu: world.c
  m@/var/tmp/hello/hello.h:1:12
  main@/var/tmp/hello/world.c:14:1
   argc@/var/tmp/hello/world.c:14:11
   argv@/var/tmp/hello/world.c:14:30
   n@/var/tmp/hello/world.c:16:15
  calc@/var/tmp/hello/world.c:5:1
   word@/var/tmp/hello/world.c:5:19
   frob@/var/tmp/hello/hello.h:5:1
    b@/var/tmp/hello/hello.h:5:18
    a@/var/tmp/hello/hello.h:5:11
    c@/var/tmp/hello/hello.h:7:7
  frob@/var/tmp/hello/hello.h:5:1
   a@/var/tmp/hello/hello.h:5:11
   b@/var/tmp/hello/hello.h:5:18
   c@/var/tmp/hello/hello.h:7:7
  exit@/usr/include/stdlib.h:542:13
  baz@/var/tmp/hello/hello.h:2:12
EOF

testrun_compare ${abs_builddir}/declfiles testfile-splitdwarf-4 << EOF
file: testfile-splitdwarf-4
 cu: hello.c
  wchar_t@/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h:328:24
  m@/home/mark/src/elfutils/tests/hello.h:1:12
  m@/home/mark/src/elfutils/tests/hello.c:5:5
  foo@/home/mark/src/elfutils/tests/hello.c:20:1
   f@/home/mark/src/elfutils/tests/hello.c:20:14
  baz@/home/mark/src/elfutils/tests/hello.c:8:5
   x@/home/mark/src/elfutils/tests/hello.c:8:14
   r@/home/mark/src/elfutils/tests/hello.c:10:7
  frob@/home/mark/src/elfutils/tests/hello.h:5:1
   a@/home/mark/src/elfutils/tests/hello.h:5:11
   b@/home/mark/src/elfutils/tests/hello.h:5:18
   c@/home/mark/src/elfutils/tests/hello.h:7:7
  foo@/home/mark/src/elfutils/tests/hello.c:20:1
   f@/home/mark/src/elfutils/tests/hello.c:20:14
   frob@/home/mark/src/elfutils/tests/hello.h:5:1
    b@/home/mark/src/elfutils/tests/hello.h:5:18
    a@/home/mark/src/elfutils/tests/hello.h:5:11
    c@/home/mark/src/elfutils/tests/hello.h:7:7
    baz@/home/mark/src/elfutils/tests/hello.c:8:5
     x@/home/mark/src/elfutils/tests/hello.c:8:14
     r@/home/mark/src/elfutils/tests/hello.c:10:7
   foo@/home/mark/src/elfutils/tests/hello.c:20:1
    f@/home/mark/src/elfutils/tests/hello.c:20:14
    main@/home/mark/src/elfutils/tests/hello.c:4:12
  baz@/home/mark/src/elfutils/tests/hello.c:8:5
   x@/home/mark/src/elfutils/tests/hello.c:8:14
   r@/home/mark/src/elfutils/tests/hello.c:10:7
   foo@/home/mark/src/elfutils/tests/hello.c:20:1
  main@/home/mark/src/elfutils/tests/hello.c:4:12
 cu: world.c
  m@/home/mark/src/elfutils/tests/hello.h:1:12
  main@/home/mark/src/elfutils/tests/world.c:14:1
   argc@/home/mark/src/elfutils/tests/world.c:14:11
   argv@/home/mark/src/elfutils/tests/world.c:14:30
   n@/home/mark/src/elfutils/tests/world.c:16:15
   calc@/home/mark/src/elfutils/tests/world.c:5:1
   exit@/usr/include/stdlib.h:542:13
  calc@/home/mark/src/elfutils/tests/world.c:5:1
   word@/home/mark/src/elfutils/tests/world.c:5:19
   frob@/home/mark/src/elfutils/tests/hello.h:5:1
    b@/home/mark/src/elfutils/tests/hello.h:5:18
    a@/home/mark/src/elfutils/tests/hello.h:5:11
    c@/home/mark/src/elfutils/tests/hello.h:7:7
    baz@/home/mark/src/elfutils/tests/hello.h:2:12
  frob@/home/mark/src/elfutils/tests/hello.h:5:1
   a@/home/mark/src/elfutils/tests/hello.h:5:11
   b@/home/mark/src/elfutils/tests/hello.h:5:18
   c@/home/mark/src/elfutils/tests/hello.h:7:7
  exit@/usr/include/stdlib.h:542:13
  baz@/home/mark/src/elfutils/tests/hello.h:2:12
EOF

testrun_compare ${abs_builddir}/declfiles testfile-splitdwarf-5 << EOF
file: testfile-splitdwarf-5
 cu: hello.c
  wchar_t@/opt/local/install/gcc/lib/gcc/x86_64-pc-linux-gnu/9.0.0/include/stddef.h:328:24
  m@/home/mark/src/elfutils/tests/hello.h:1:12
  m@/home/mark/src/elfutils/tests/hello.c:5:5
  foo@/home/mark/src/elfutils/tests/hello.c:20:1
   f@/home/mark/src/elfutils/tests/hello.c:20:14
  baz@/home/mark/src/elfutils/tests/hello.c:8:5
   x@/home/mark/src/elfutils/tests/hello.c:8:14
   r@/home/mark/src/elfutils/tests/hello.c:10:7
  frob@/home/mark/src/elfutils/tests/hello.h:5:1
   a@/home/mark/src/elfutils/tests/hello.h:5:11
   b@/home/mark/src/elfutils/tests/hello.h:5:18
   c@/home/mark/src/elfutils/tests/hello.h:7:7
  foo@/home/mark/src/elfutils/tests/hello.c:20:1
   f@/home/mark/src/elfutils/tests/hello.c:20:14
   frob@/home/mark/src/elfutils/tests/hello.h:5:1
    b@/home/mark/src/elfutils/tests/hello.h:5:18
    a@/home/mark/src/elfutils/tests/hello.h:5:11
    c@/home/mark/src/elfutils/tests/hello.h:7:7
    baz@/home/mark/src/elfutils/tests/hello.c:8:5
     x@/home/mark/src/elfutils/tests/hello.c:8:14
     r@/home/mark/src/elfutils/tests/hello.c:10:7
   foo@/home/mark/src/elfutils/tests/hello.c:20:1
    f@/home/mark/src/elfutils/tests/hello.c:20:14
  baz@/home/mark/src/elfutils/tests/hello.c:8:5
   x@/home/mark/src/elfutils/tests/hello.c:8:14
   r@/home/mark/src/elfutils/tests/hello.c:10:7
  main@/home/mark/src/elfutils/tests/hello.c:4:12
 cu: world.c
  m@/home/mark/src/elfutils/tests/hello.h:1:12
  main@/home/mark/src/elfutils/tests/world.c:14:1
   argc@/home/mark/src/elfutils/tests/world.c:14:11
   argv@/home/mark/src/elfutils/tests/world.c:14:30
   n@/home/mark/src/elfutils/tests/world.c:16:15
  calc@/home/mark/src/elfutils/tests/world.c:5:1
   word@/home/mark/src/elfutils/tests/world.c:5:19
   frob@/home/mark/src/elfutils/tests/hello.h:5:1
    b@/home/mark/src/elfutils/tests/hello.h:5:18
    a@/home/mark/src/elfutils/tests/hello.h:5:11
    c@/home/mark/src/elfutils/tests/hello.h:7:7
  frob@/home/mark/src/elfutils/tests/hello.h:5:1
   a@/home/mark/src/elfutils/tests/hello.h:5:11
   b@/home/mark/src/elfutils/tests/hello.h:5:18
   c@/home/mark/src/elfutils/tests/hello.h:7:7
  exit@/usr/include/stdlib.h:542:13
  baz@/home/mark/src/elfutils/tests/hello.h:2:12
EOF

# See testfile-dwp.source.
testfiles testfile-dwp-5 testfile-dwp-5.dwp
testfiles testfile-dwp-4 testfile-dwp-4.dwp

testrun_compare ${abs_builddir}/declfiles testfile-dwp-5 << EOF
file: testfile-dwp-5
 cu: foo.cc
  foo@/home/osandov/src/elfutils/tests/foobar.h:4:7
  foo@/home/osandov/src/elfutils/tests/foo.cc:14:1
   x@/home/osandov/src/elfutils/tests/foo.cc:16:7
   x_x@/home/osandov/src/elfutils/tests/foo.cc:6:1
    x@/home/osandov/src/elfutils/tests/foo.cc:6:10
    i@/home/osandov/src/elfutils/tests/foo.cc:8:12
  x_x@/home/osandov/src/elfutils/tests/foo.cc:6:1
   x@/home/osandov/src/elfutils/tests/foo.cc:6:10
   i@/home/osandov/src/elfutils/tests/foo.cc:8:12
  x_x@/home/osandov/src/elfutils/tests/foo.cc:6:1
   x@/home/osandov/src/elfutils/tests/foo.cc:6:10
   i@/home/osandov/src/elfutils/tests/foo.cc:8:12
 cu: bar.cc
  bar@/home/osandov/src/elfutils/tests/foobar.h:10:8
  bar@/home/osandov/src/elfutils/tests/bar.cc:6:1
 cu: main.cc
  foo@/home/osandov/src/elfutils/tests/foobar.h:4:7
  bar@/home/osandov/src/elfutils/tests/foobar.h:10:8
  main@/home/osandov/src/elfutils/tests/main.cc:6:1
   argc@/home/osandov/src/elfutils/tests/main.cc:6:6
   argv@/home/osandov/src/elfutils/tests/main.cc:6:6
   myfoo@/home/osandov/src/elfutils/tests/main.cc:8:14
   mybar@/home/osandov/src/elfutils/tests/main.cc:9:14
   fibonacci@/home/osandov/src/elfutils/tests/foobar.h:17:1
    n@/home/osandov/src/elfutils/tests/foobar.h:17:25
    fibonacci@/home/osandov/src/elfutils/tests/foobar.h:17:1
     n@/home/osandov/src/elfutils/tests/foobar.h:17:25
     a@/home/osandov/src/elfutils/tests/foobar.h:23:12
     b@/home/osandov/src/elfutils/tests/foobar.h:24:12
     i@/home/osandov/src/elfutils/tests/foobar.h:25:25
     tmp@/home/osandov/src/elfutils/tests/foobar.h:27:9
  fibonacci@/home/osandov/src/elfutils/tests/foobar.h:17:1
   n@/home/osandov/src/elfutils/tests/foobar.h:17:25
   a@/home/osandov/src/elfutils/tests/foobar.h:23:12
   b@/home/osandov/src/elfutils/tests/foobar.h:24:12
   i@/home/osandov/src/elfutils/tests/foobar.h:25:25
   tmp@/home/osandov/src/elfutils/tests/foobar.h:27:9
EOF

testrun_compare ${abs_builddir}/declfiles testfile-dwp-4 << EOF
file: testfile-dwp-4
 cu: foo.cc
  foo@/home/osandov/src/elfutils/tests/foobar.h:4:7
  foo@/home/osandov/src/elfutils/tests/foo.cc:14:1
   x@/home/osandov/src/elfutils/tests/foo.cc:16:7
   x_x@/home/osandov/src/elfutils/tests/foo.cc:6:1
    x@/home/osandov/src/elfutils/tests/foo.cc:6:10
    i@/home/osandov/src/elfutils/tests/foo.cc:8:12
  x_x@/home/osandov/src/elfutils/tests/foo.cc:6:1
   x@/home/osandov/src/elfutils/tests/foo.cc:6:10
   i@/home/osandov/src/elfutils/tests/foo.cc:8:12
  x_x@/home/osandov/src/elfutils/tests/foo.cc:6:1
   x@/home/osandov/src/elfutils/tests/foo.cc:6:10
   i@/home/osandov/src/elfutils/tests/foo.cc:8:12
 cu: bar.cc
  bar@/home/osandov/src/elfutils/tests/foobar.h:10:8
  bar@/home/osandov/src/elfutils/tests/bar.cc:6:1
 cu: main.cc
  foo@/home/osandov/src/elfutils/tests/foobar.h:4:7
  bar@/home/osandov/src/elfutils/tests/foobar.h:10:8
  main@/home/osandov/src/elfutils/tests/main.cc:6:1
   argc@/home/osandov/src/elfutils/tests/main.cc:6:6
   argv@/home/osandov/src/elfutils/tests/main.cc:6:6
   myfoo@/home/osandov/src/elfutils/tests/main.cc:8:14
   mybar@/home/osandov/src/elfutils/tests/main.cc:9:14
   fibonacci@/home/osandov/src/elfutils/tests/foobar.h:17:1
    n@/home/osandov/src/elfutils/tests/foobar.h:17:25
    fibonacci@/home/osandov/src/elfutils/tests/foobar.h:17:1
     n@/home/osandov/src/elfutils/tests/foobar.h:17:25
     a@/home/osandov/src/elfutils/tests/foobar.h:23:12
     b@/home/osandov/src/elfutils/tests/foobar.h:24:12
     i@/home/osandov/src/elfutils/tests/foobar.h:25:25
     tmp@/home/osandov/src/elfutils/tests/foobar.h:27:9
   foo@/home/osandov/src/elfutils/tests/foobar.h:4:7
   bar@/home/osandov/src/elfutils/tests/foobar.h:10:8
  fibonacci@/home/osandov/src/elfutils/tests/foobar.h:17:1
   n@/home/osandov/src/elfutils/tests/foobar.h:17:25
   a@/home/osandov/src/elfutils/tests/foobar.h:23:12
   b@/home/osandov/src/elfutils/tests/foobar.h:24:12
   i@/home/osandov/src/elfutils/tests/foobar.h:25:25
   tmp@/home/osandov/src/elfutils/tests/foobar.h:27:9
EOF
