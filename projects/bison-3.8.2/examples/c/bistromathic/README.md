# bistromathic - all the bells and whistles
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

To customize the interaction with bistromathic, see the GNU Readline user
manual (see `info rluserman`).

<!---
Local Variables:
fill-column: 76
ispell-dictionary: "american"
End:

Copyright (C) 2020-2021 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.3 or
any later version published by the Free Software Foundation; with no
Invariant Sections, with no Front-Cover Texts, and with no Back-Cover
Texts.  A copy of the license is included in the "GNU Free
Documentation License" file as part of this distribution.

LocalWords:  bistromathic yyexpected lookahead ispell american
LocalWords:  MERCHANTABILITY

--->
