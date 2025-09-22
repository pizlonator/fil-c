# bench.pl
A primitive Perl script to run benches.  Currently its only bench is the
usual calculator: it has a lightweight processing part (but not null), so it
is really emphasizing the cost of the whole parsing (including scanning and
I/O).

Set the envvars `BISON` and `CC` to the programs and flags you want to use.
They default to `bison` and `gcc`.  To run a non-installed bison from a
tarball built in _build, I suggest the following.

    BISON=_build/tests/bison CC='gcc -O2' ./bench.pl

(The `_build/tests/bison` wrapper makes sure that `_build/src/bison` will
use the tarballs' skeletons, not those already installed as a
straightforward use of `_build/src/bison` would.)

<!--

Local Variables:
mode: markdown
fill-column: 76
ispell-dictionary: "american"
End:

Copyright (C) 2006, 2009-2015, 2018-2021 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.3 or
any later version published by the Free Software Foundation; with no
Invariant Sections, with no Front-Cover Texts, and with no Back-Cover
Texts.  A copy of the license is included in the "GNU Free
Documentation License" file as part of this distribution.

-->
