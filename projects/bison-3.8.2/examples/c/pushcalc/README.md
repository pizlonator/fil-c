# pushcalc - push parser with Bison

This directory contains pushcalc, the traditional calculator, implemented as
a push parser.

Traditionally Bison is used to create so called "pull parsers": the user
invokes the parser once, which repeatedly calls (pulls) the scanner until
the input is drained.

This example demonstrates the "push parsers": the user calls scanner to
fetch the next token, passes (pushes) it to the parser, and repeats the
operation until the input is drained.

This example is a straightforward conversion of the 'calc' example to the
push-parser model.

<!---
Local Variables:
fill-column: 76
ispell-dictionary: "american"
End:

Copyright (C) 2020-2021 Free Software Foundation, Inc.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.3 or
any later version published by the Free Software Foundation; with no
Invariant Sections, with no Front-Cover Texts, and with no Back-Cover
Texts.  A copy of the license is included in the "GNU Free
Documentation License" file as part of this distribution.

--->
