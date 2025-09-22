# calc++ - A Flex+Bison calculator

This directory contains calc++, a Bison grammar file in C++.  If you never
saw the traditional implementation in C, please first read
examples/c/lexcalc, which can be seen as a C precursor of this example.

Read the corresponding chapter in the documentation: "A Complete C++
Example".  It is also available [on
line](https://www.gnu.org/software/bison/manual/html_node/A-Complete-C_002b_002b-Example.html)
(maybe with a different version of Bison).

To use it, copy this directory into some work directory, and run `make` to
compile the executable, and try it.  It is a simple calculator which accepts
several variable definitions, one per line, and then a single expression to
evaluate.

The program calc++ expects the file to parse as argument; pass `-` to read
the standard input (and then hit <Ctrl-d>, control-d, to end your input).

```
$ ./calc++ -
one := 1
two := 2
three := 3
(one + two * three) * two * three
<Ctrl-d>
42
```

You may pass `-p` to activate the parser debug traces, and `-s` to activate
the scanner's.

<!---
Local Variables:
fill-column: 76
ispell-dictionary: "american"
End:

Copyright (C) 2018-2021 Free Software Foundation, Inc.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.3 or
any later version published by the Free Software Foundation; with no
Invariant Sections, with no Front-Cover Texts, and with no Back-Cover
Texts.  A copy of the license is included in the "GNU Free
Documentation License" file as part of this distribution.

LocalWords:  calc parsers yy MERCHANTABILITY Ctrl ispell american
--->
