# reccalc - recursive calculator with Flex and Bison

In this example the generated parser is pure and reentrant: it can be used
concurrently in different threads, or recursively.  As a proof of this
reentrancy, expressions in parenthesis are tokenized as strings, and then
recursively parsed from the parser:

```
exp: STR
  {
    result r = parse_string ($1);
    free ($1);
    if (r.nerrs)
      {
        res->nerrs += r.nerrs;
        YYERROR;
      }
    else
      $$ = r.value;
  }
```

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

--->
