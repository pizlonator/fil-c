#! /usr/bin/python3
# Approximation to C preprocessing.
# Copyright (C) 2019-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

"""
Simplified lexical analyzer for C preprocessing tokens.

Does not implement trigraphs.

Does not implement backslash-newline in the middle of any lexical
item other than a string literal.

Does not implement universal-character-names in identifiers.

Treats prefixed strings (e.g. L"...") as two tokens (L and "...").

Accepts non-ASCII characters only within comments and strings.
"""

import collections
import operator
import re
import sys

# Caution: The order of the outermost alternation matters.
# STRING must be before BAD_STRING, CHARCONST before BAD_CHARCONST,
# BLOCK_COMMENT before BAD_BLOCK_COM before PUNCTUATOR, and OTHER must
# be last.
# Caution: There should be no capturing groups other than the named
# captures in the outermost alternation.

# For reference, these are all of the C punctuators as of C11:
#   [ ] ( ) { } , ; ? ~
#   ! != * *= / /= ^ ^= = ==
#   # ##
#   % %= %> %: %:%:
#   & &= &&
#   | |= ||
#   + += ++
#   - -= -- ->
#   . ...
#   : :>
#   < <% <: << <<= <=
#   > >= >> >>=

# The BAD_* tokens are not part of the official definition of pp-tokens;
# they match unclosed strings, character constants, and block comments,
# so that the regex engine doesn't have to backtrack all the way to the
# beginning of a broken construct and then emit dozens of junk tokens.

PP_TOKEN_RE_ = re.compile(r"""
    (?P<STRING>        \"(?:[^\"\\\r\n]|\\(?:[\r\n -~]|\r\n))*\")
   |(?P<BAD_STRING>    \"(?:[^\"\\\r\n]|\\[ -~])*)
   |(?P<CHARCONST>     \'(?:[^\'\\\r\n]|\\(?:[\r\n -~]|\r\n))*\')
   |(?P<BAD_CHARCONST> \'(?:[^\'\\\r\n]|\\[ -~])*)
   |(?P<BLOCK_COMMENT> /\*(?:\*(?!/)|[^*])*\*/)
   |(?P<BAD_BLOCK_COM> /\*(?:\*(?!/)|[^*])*\*?)
   |(?P<LINE_COMMENT>  //[^\r\n]*)
   |(?P<IDENT>         [_a-zA-Z][_a-zA-Z0-9]*)
   |(?P<PP_NUMBER>     \.?[0-9](?:[0-9a-df-oq-zA-DF-OQ-Z_.]|[eEpP][+-]?)*)
   |(?P<PUNCTUATOR>
       [,;?~(){}\[\]]
     | [!*/^=]=?
     | \#\#?
     | %(?:[=>]|:(?:%:)?)?
     | &[=&]?
     |\|[=|]?
     |\+[=+]?
     | -[=->]?
     |\.(?:\.\.)?
     | :>?
     | <(?:[%:]|<(?:=|<=?)?)?
     | >(?:=|>=?)?)
   |(?P<ESCNL>         \\(?:\r|\n|\r\n))
   |(?P<WHITESPACE>    [ \t\n\r\v\f]+)
   |(?P<OTHER>         .)
""", re.DOTALL | re.VERBOSE)

HEADER_NAME_RE_ = re.compile(r"""
    < [^>\r\n]+ >
  | " [^"\r\n]+ "
""", re.DOTALL | re.VERBOSE)

ENDLINE_RE_ = re.compile(r"""\r|\n|\r\n""")

# based on the sample code in the Python re documentation
Token_ = collections.namedtuple("Token", (
    "kind", "text", "line", "column", "context"))
Token_.__doc__ = """
   One C preprocessing token, comment, or chunk of whitespace.
   'kind' identifies the token type, which will be one of:
       STRING, CHARCONST, BLOCK_COMMENT, LINE_COMMENT, IDENT,
       PP_NUMBER, PUNCTUATOR, ESCNL, WHITESPACE, HEADER_NAME,
       or OTHER.  The BAD_* alternatives in PP_TOKEN_RE_ are
       handled within tokenize_c, below.

   'text' is the sequence of source characters making up the token;
       no decoding whatsoever is performed.

   'line' and 'column' give the position of the first character of the
      token within the source file.  They are both 1-based.

   'context' indicates whether or not this token occurred within a
      preprocessing directive; it will be None for running text,
      '<null>' for the leading '#' of a directive line (because '#'
      all by itself on a line is a "null directive"), or the name of
      the directive for tokens within a directive line, starting with
      the IDENT for the name itself.
"""

def tokenize_c(file_contents, reporter):
    """Yield a series of Token objects, one for each preprocessing
       token, comment, or chunk of whitespace within FILE_CONTENTS.
       The REPORTER object is expected to have one method,
       reporter.error(token, message), which will be called to
       indicate a lexical error at the position of TOKEN.
       If MESSAGE contains the four-character sequence '{!r}', that
       is expected to be replaced by repr(token.text).
    """

    Token = Token_
    PP_TOKEN_RE = PP_TOKEN_RE_
    ENDLINE_RE = ENDLINE_RE_
    HEADER_NAME_RE = HEADER_NAME_RE_

    line_num = 1
    line_start = 0
    pos = 0
    limit = len(file_contents)
    directive = None
    at_bol = True
    while pos < limit:
        if directive == "include":
            mo = HEADER_NAME_RE.match(file_contents, pos)
            if mo:
                kind = "HEADER_NAME"
                directive = "after_include"
            else:
                mo = PP_TOKEN_RE.match(file_contents, pos)
                kind = mo.lastgroup
                if kind != "WHITESPACE":
                    directive = "after_include"
        else:
            mo = PP_TOKEN_RE.match(file_contents, pos)
            kind = mo.lastgroup

        text = mo.group()
        line = line_num
        column = mo.start() - line_start
        adj_line_start = 0
        # only these kinds can contain a newline
        if kind in ("WHITESPACE", "BLOCK_COMMENT", "LINE_COMMENT",
                    "STRING", "CHARCONST", "BAD_BLOCK_COM", "ESCNL"):
            for tmo in ENDLINE_RE.finditer(text):
                line_num += 1
                adj_line_start = tmo.end()
            if adj_line_start:
                line_start = mo.start() + adj_line_start

        # Track whether or not we are scanning a preprocessing directive.
        if kind == "LINE_COMMENT" or (kind == "WHITESPACE" and adj_line_start):
            at_bol = True
            directive = None
        else:
            if kind == "PUNCTUATOR" and text == "#" and at_bol:
                directive = "<null>"
            elif kind == "IDENT" and directive == "<null>":
                directive = text
            at_bol = False

        # Report ill-formed tokens and rewrite them as their well-formed
        # equivalents, so downstream processing doesn't have to know about them.
        # (Rewriting instead of discarding provides better error recovery.)
        if kind == "BAD_BLOCK_COM":
            reporter.error(Token("BAD_BLOCK_COM", "", line, column+1, ""),
                           "unclosed block comment")
            text += "*/"
            kind = "BLOCK_COMMENT"
        elif kind == "BAD_STRING":
            reporter.error(Token("BAD_STRING", "", line, column+1, ""),
                           "unclosed string")
            text += "\""
            kind = "STRING"
        elif kind == "BAD_CHARCONST":
            reporter.error(Token("BAD_CHARCONST", "", line, column+1, ""),
                           "unclosed char constant")
            text += "'"
            kind = "CHARCONST"

        tok = Token(kind, text, line, column+1,
                    "include" if directive == "after_include" else directive)
        # Do not complain about OTHER tokens inside macro definitions.
        # $ and @ appear in macros defined by headers intended to be
        # included from assembly language, e.g. sysdeps/mips/sys/asm.h.
        if kind == "OTHER" and directive != "define":
            self.error(tok, "stray {!r} in program")

        yield tok
        pos = mo.end()

class MacroDefinition(collections.namedtuple('MacroDefinition',
                                             'name_token args body error')):
    """A preprocessor macro definition.

    name_token is the Token_ for the name.

    args is None for a macro that is not function-like.  Otherwise, it
    is a tuple that contains the macro argument name tokens.

    body is a tuple that contains the tokens that constitute the body
    of the macro definition (excluding whitespace).

    error is None if no error was detected, or otherwise a problem
    description associated with this macro definition.

    """

    @property
    def function(self):
        """Return true if the macro is function-like."""
        return self.args is not None

    @property
    def name(self):
        """Return the name of the macro being defined."""
        return self.name_token.text

    @property
    def line(self):
        """Return the line number of the macro definition."""
        return self.name_token.line

    @property
    def args_lowered(self):
        """Return the macro argument list as a list of strings"""
        if self.function:
            return [token.text for token in self.args]
        else:
            return None

    @property
    def body_lowered(self):
        """Return the macro body as a list of strings."""
        return [token.text for token in self.body]

def macro_definitions(tokens):
    """A generator for C macro definitions among tokens.

    The generator yields MacroDefinition objects.

    tokens must be iterable, yielding Token_ objects.

    """

    macro_name = None
    macro_start = False # Set to false after macro name and one otken.
    macro_args = None # Set to a list during the macro argument sequence.
    in_macro_args = False # True while processing macro identifier-list.
    error = None
    body = []

    for token in tokens:
        if token.context == 'define' and macro_name is None \
           and token.kind == 'IDENT':
            # Starting up macro processing.
            if macro_start:
                # First identifier is the macro name.
                macro_name = token
            else:
                # Next token is the name.
                macro_start = True
            continue

        if macro_name is None:
            # Drop tokens not in macro definitions.
            continue

        if token.context != 'define':
            # End of the macro definition.
            if in_macro_args and error is None:
                error = 'macro definition ends in macro argument list'
            yield MacroDefinition(macro_name, macro_args, tuple(body), error)
            # No longer in a macro definition.
            macro_name = None
            macro_start = False
            macro_args = None
            in_macro_args = False
            error = None
            body.clear()
            continue

        if macro_start:
            # First token after the macro name.
            macro_start = False
            if token.kind == 'PUNCTUATOR' and token.text == '(':
                macro_args = []
                in_macro_args = True
            continue

        if in_macro_args:
            if token.kind == 'IDENT' \
               or (token.kind == 'PUNCTUATOR' and token.text == '...'):
                # Macro argument or ... placeholder.
                macro_args.append(token)
            if token.kind == 'PUNCTUATOR':
                if token.text == ')':
                    macro_args = tuple(macro_args)
                    in_macro_args = False
                elif token.text == ',':
                    pass # Skip.  Not a full syntax check.
                elif error is None:
                    error = 'invalid punctuator in macro argument list: ' \
                        + repr(token.text)
            elif error is None:
                error = 'invalid {} token in macro argument list'.format(
                    token.kind)
            continue

        if token.kind not in ('WHITESPACE', 'BLOCK_COMMENT'):
            body.append(token)

    # Emit the macro in case the last line does not end with a newline.
    if macro_name is not None:
        if in_macro_args and error is None:
            error = 'macro definition ends in macro argument list'
        yield MacroDefinition(macro_name, macro_args, tuple(body), error)

# Used to split UL etc. suffixes from numbers such as 123UL.
RE_SPLIT_INTEGER_SUFFIX = re.compile(r'([^ullULL]+)([ullULL]*)')

BINARY_OPERATORS = {
    '+': operator.add,
    '<<': operator.lshift,
    '|': operator.or_,
}

# Use the general-purpose dict type if it is order-preserving.
if (sys.version_info[0], sys.version_info[1]) <= (3, 6):
    OrderedDict = collections.OrderedDict
else:
    OrderedDict = dict

def macro_eval(macro_defs, reporter):
    """Compute macro values

    macro_defs is the output from macro_definitions.  reporter is an
    object that accepts reporter.error(line_number, message) and
    reporter.note(line_number, message) calls to report errors
    and error context invocations.

    The returned dict contains the values of macros which are not
    function-like, pairing their names with their computed values.

    The current implementation is incomplete.  It is deliberately not
    entirely faithful to C, even in the implemented parts.  It checks
    that macro replacements follow certain syntactic rules even if
    they are never evaluated.

    """

    # Unevaluated macro definitions by name.
    definitions = OrderedDict()
    for md in macro_defs:
        if md.name in definitions:
            reporter.error(md.line, 'macro {} redefined'.format(md.name))
            reporter.note(definitions[md.name].line,
                          'location of previous definition')
        else:
            definitions[md.name] = md

    # String to value mappings for fully evaluated macros.
    evaluated = OrderedDict()

    # String to macro definitions during evaluation.  Nice error
    # reporting relies on deterministic iteration order.
    stack = OrderedDict()

    def eval_token(current, token):
        """Evaluate one macro token.

        Integers and strings are returned as such (the latter still
        quoted).  Identifiers are expanded.

        None indicates an empty expansion or an error.

        """

        if token.kind == 'PP_NUMBER':
            value = None
            m = RE_SPLIT_INTEGER_SUFFIX.match(token.text)
            if m:
                try:
                    value = int(m.group(1), 0)
                except ValueError:
                    pass
            if value is None:
                reporter.error(token.line,
                    'invalid number {!r} in definition of {}'.format(
                        token.text, current.name))
            return value

        if token.kind == 'STRING':
            return token.text

        if token.kind == 'CHARCONST' and len(token.text) == 3:
            return ord(token.text[1])

        if token.kind == 'IDENT':
            name = token.text
            result = eval1(current, name)
            if name not in evaluated:
                evaluated[name] = result
            return result

        reporter.error(token.line,
            'unrecognized {!r} in definition of {}'.format(
                token.text, current.name))
        return None


    def eval1(current, name):
        """Evaluate one name.

        The name is looked up and the macro definition evaluated
        recursively if necessary.  The current argument is the macro
        definition being evaluated.

        None as a return value indicates an error.

        """

        # Fast path if the value has already been evaluated.
        if name in evaluated:
            return evaluated[name]

        try:
            md = definitions[name]
        except KeyError:
            reporter.error(current.line,
                'reference to undefined identifier {} in definition of {}'
                           .format(name, current.name))
            return None

        if md.name in stack:
            # Recursive macro definition.
            md = stack[name]
            reporter.error(md.line,
                'macro definition {} refers to itself'.format(md.name))
            for md1 in reversed(list(stack.values())):
                if md1 is md:
                    break
                reporter.note(md1.line,
                              'evaluated from {}'.format(md1.name))
            return None

        stack[md.name] = md
        if md.function:
            reporter.error(current.line,
                'attempt to evaluate function-like macro {}'.format(name))
            reporter.note(md.line, 'definition of {}'.format(md.name))
            return None

        try:
            body = md.body
            if len(body) == 0:
                # Empty expansion.
                return None

            # Remove surrounding ().
            if body[0].text == '(' and body[-1].text == ')':
                body = body[1:-1]
                had_parens = True
            else:
                had_parens = False

            if len(body) == 1:
                return eval_token(md, body[0])

            # Minimal expression evaluator for binary operators.
            op = body[1].text
            if len(body) == 3 and op in BINARY_OPERATORS:
                if not had_parens:
                    reporter.error(body[1].line,
                        'missing parentheses around {} expression'.format(op))
                    reporter.note(md.line,
                                  'in definition of macro {}'.format(md.name))

                left = eval_token(md, body[0])
                right = eval_token(md, body[2])

                if type(left) != type(1):
                    reporter.error(left.line,
                        'left operand of {} is not an integer'.format(op))
                    reporter.note(md.line,
                                  'in definition of macro {}'.format(md.name))
                if type(right) != type(1):
                    reporter.error(left.line,
                        'right operand of {} is not an integer'.format(op))
                    reporter.note(md.line,
                                  'in definition of macro {}'.format(md.name))
                return BINARY_OPERATORS[op](left, right)

            reporter.error(md.line,
                'uninterpretable macro token sequence: {}'.format(
                    ' '.join(md.body_lowered)))
            return None
        finally:
            del stack[md.name]

    # Start of main body of macro_eval.
    for md in definitions.values():
        name = md.name
        if name not in evaluated and not md.function:
            evaluated[name] = eval1(md, name)
    return evaluated
