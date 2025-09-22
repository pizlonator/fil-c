# Examples in C

This directory contains simple examples of Bison grammar files in C.

Some of them come from the documentation, which should be installed together
with Bison.  The URLs are provided for convenience.

## rpcalc - Reverse Polish Notation Calculator
The first example is that of a simple double-precision Reverse Polish
Notation calculator (a calculator using postfix operators). This example
provides a good starting point, since operator precedence is not an issue.

Extracted from the documentation: [Reverse Polish Notation
Calculator](https://www.gnu.org/software/bison/manual/html_node/RPN-Calc.html).

## calc - Simple Calculator
This example is slightly more complex than rpcalc: it features infix
operators (`1 + 2`, instead of `1 2 +` in rpcalc), but it does so using a
unambiguous grammar of the arithmetic instead of using precedence
directives (%left, etc.).

## mfcalc - Multi-Function Calculator
A more complete C example: a multi-function calculator.  More complex than
the previous example.  Using precedence directives to support infix
operators.

Extracted from the documentation: [Multi-Function Calculator:
mfcalc](https://www.gnu.org/software/bison/manual/html_node/Multi_002dfunction-Calc.html).

## lexcalc - calculator with Flex and Bison
The calculator with precedence directives and location tracking.  It uses
Flex to generate the scanner.

## reccalc - recursive calculator with Flex and Bison
This example builds on top of the previous one to provide a reentrant
parser.  Such parsers can be called concurrently in different threads, or
even recursively.  To demonstrate this feature, expressions in parentheses
are tokenized as strings, and then recursively parsed from the parser.  So
`(((1)+(2))*((3)+(4)))` uses eight parsers, with a depth of four.

## pushcalc - calculator implemented with a push parser
All the previous examples are so called "pull parsers": the user invokes the
parser once, which repeatedly calls the scanner until the input is drained.

This example demonstrates the "push parsers": the user calls the scanner to
fetch the next token, passes it to the parser, and repeats the operation
until the input is drained.

This example is a straightforward conversion of the 'calc' example to the
push-parser model.

## bistromathic - all the bells and whistles
This example demonstrates best practices when using Bison.
- Its hand-written scanner tracks locations.
- Its interface is pure.
- It uses %params to pass user information to the parser and scanner.
- Its scanner uses the `error` token to signal lexical errors and enter
  error recovery.
- Its interface is "incremental", well suited for interaction: it uses the
  push-parser API to feed the parser with the incoming tokens.
- It features an interactive command line with completion based on the
  parser state, based on `yyexpected_tokens`.
- It uses Bison's standard catalog for internationalization of generated
  messages.
- It uses a custom syntax error with location, lookahead correction and
  token internationalization.
- Error messages quote the source with squiggles that underline the error:
```
> 123 456
1.5-7: syntax error: expected end of file or + or - or * or / or ^ before number
    1 | 123 456
      |     ^~~
```
- It supports debug traces with semantic values.
- It uses named references instead of the traditional $1, $2, etc.

<!---

Local Variables:
fill-column: 76
ispell-dictionary: "american"
End:

Copyright (C) 2018-2021 Free Software Foundation, Inc.

This file is part of GNU bison, the GNU Compiler Compiler.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.3 or
any later version published by the Free Software Foundation; with no
Invariant Sections, with no Front-Cover Texts, and with no Back-Cover
Texts.  A copy of the license is included in the "GNU Free
Documentation License" file as part of this distribution.

LocalWords:  mfcalc calc parsers yy rpcalc lexcalc redux reccalc ispell
LocalWords:  reentrant tokenized american postfix pushcalc bistromathic
LocalWords:  lookahead

-->
