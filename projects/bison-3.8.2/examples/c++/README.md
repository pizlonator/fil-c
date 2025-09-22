# Examples in C++

This directory contains examples of Bison grammar files in C++.

You can run `make` to compile these examples.  And `make clean` to tidy
afterwards.

## simple.yy - Simple example in C++14
A very simple example in C++, based on variants and symbol constructors.
Variants allow to use any C++ type as semantic value type, and symbol
constructors ensure consistency between declared token kind and effective
semantic value.

Run as `./simple`.

Extracted from the documentation: [A Simple C++
Example](https://www.gnu.org/software/bison/manual/html_node/A-Simple-C_002b_002b-Example.html).

## variant.yy - Self-contained example in C++98
A variation of simple.yy, in C++98.

Run as `./variant`.

## variant-11.yy - Self-contained example in modern C++
Another variation of simple.yy, closely related to the previous one, but
exhibiting support for C++11's move semantics.

Run as `./variant` or `./variant NUMBER`.

## calc++ - A Complete C++ Example
A fully featured C++ version of the canonical example for parsers: a
calculator.  Also uses Flex for the scanner.

Don't look at this example first: it is fully featured and can serve as a
starting point for a clean parser in C++.  The previous examples are better
introductory examples, and the C examples are also useful introductory
examples.

Extracted from the documentation: [A Complete C++
Example](https://www.gnu.org/software/bison/manual/html_node/A-Complete-C_002b_002b-Example.html).

## glr

This example demonstrates the use of GLR parsers to handle (local)
ambiguities in the C++ language.  See the node "Merging GLR Parses" in
Bison's documentation.

It uses (Bison) variants to store objects as semantic values.  It also
demonstrates custom error messages in C++.

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

LocalWords:  mfcalc calc parsers yy ispell american
--->
