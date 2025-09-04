#! /usr/bin/python3
# Tests for scripts/glibcpp.py
# Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

import inspect
import sys

import glibcpp

# Error counter.
errors = 0

class TokenizerErrors:
    """Used as the error reporter during tokenization."""

    def __init__(self):
        self.errors = []

    def error(self, token, message):
        self.errors.append((token, message))

def check_macro_definitions(source, expected):
    reporter = TokenizerErrors()
    tokens = glibcpp.tokenize_c(source, reporter)

    actual = []
    for md in glibcpp.macro_definitions(tokens):
        if md.function:
            md_name = '{}({})'.format(md.name, ','.join(md.args_lowered))
        else:
            md_name = md.name
        actual.append((md_name, md.body_lowered))

    if actual != expected or reporter.errors:
        global errors
        errors += 1
        # Obtain python source line information.
        frame = inspect.stack(2)[1]
        print('{}:{}: error: macro definition mismatch, actual definitions:'
              .format(frame[1], frame[2]))
        for md in actual:
            print('note: {} {!r}'.format(md[0], md[1]))

        if reporter.errors:
            for err in reporter.errors:
                print('note: tokenizer error: {}: {}'.format(
                    err[0].line, err[1]))

def check_macro_eval(source, expected, expected_errors=''):
    reporter = TokenizerErrors()
    tokens = list(glibcpp.tokenize_c(source, reporter))

    if reporter.errors:
        # Obtain python source line information.
        frame = inspect.stack(2)[1]
        for err in reporter.errors:
            print('{}:{}: tokenizer error: {}: {}'.format(
                frame[1], frame[2], err[0].line, err[1]))
        return

    class EvalReporter:
        """Used as the error reporter during evaluation."""

        def __init__(self):
            self.lines = []

        def error(self, line, message):
            self.lines.append('{}: error: {}\n'.format(line, message))

        def note(self, line, message):
            self.lines.append('{}: note: {}\n'.format(line, message))

    reporter = EvalReporter()
    actual = glibcpp.macro_eval(glibcpp.macro_definitions(tokens), reporter)
    actual_errors = ''.join(reporter.lines)
    if actual != expected or actual_errors != expected_errors:
        global errors
        errors += 1
        # Obtain python source line information.
        frame = inspect.stack(2)[1]
        print('{}:{}: error: macro evaluation mismatch, actual results:'
              .format(frame[1], frame[2]))
        for k, v in actual.items():
            print('  {}: {!r}'.format(k, v))
        for msg in reporter.lines:
            sys.stdout.write('  | ' + msg)

# Individual test cases follow.

check_macro_definitions('', [])
check_macro_definitions('int main()\n{\n{\n', [])
check_macro_definitions("""
#define A 1
#define B 2 /* ignored */
#define C 3 // also ignored
#define D \
 4
#define STRING "string"
#define FUNCLIKE(a, b) (a + b)
#define FUNCLIKE2(a, b) (a + \
 b)
""", [('A', ['1']),
      ('B', ['2']),
      ('C', ['3']),
      ('D', ['4']),
      ('STRING', ['"string"']),
      ('FUNCLIKE(a,b)', list('(a+b)')),
      ('FUNCLIKE2(a,b)', list('(a+b)')),
      ])
check_macro_definitions('#define MACRO', [('MACRO', [])])
check_macro_definitions('#define MACRO\n', [('MACRO', [])])
check_macro_definitions('#define MACRO()', [('MACRO()', [])])
check_macro_definitions('#define MACRO()\n', [('MACRO()', [])])

check_macro_eval('#define A 1', {'A': 1})
check_macro_eval('#define A (1)', {'A': 1})
check_macro_eval('#define A (1 + 1)', {'A': 2})
check_macro_eval('#define A (1U << 31)', {'A': 1 << 31})
check_macro_eval('#define A (1 | 2)', {'A': 1 | 2})
check_macro_eval('''\
#define A (B + 1)
#define B 10
#define F(x) ignored
#define C "not ignored"
''', {
    'A': 11,
    'B': 10,
    'C': '"not ignored"',
})

# Checking for evaluation errors.
check_macro_eval('''\
#define A 1
#define A 2
''', {
    'A': 1,
}, '''\
2: error: macro A redefined
1: note: location of previous definition
''')

check_macro_eval('''\
#define A A
#define B 1
''', {
    'A': None,
    'B': 1,
}, '''\
1: error: macro definition A refers to itself
''')

check_macro_eval('''\
#define A B
#define B A
''', {
    'A': None,
    'B': None,
}, '''\
1: error: macro definition A refers to itself
2: note: evaluated from B
''')

check_macro_eval('''\
#define A B
#define B C
#define C A
''', {
    'A': None,
    'B': None,
    'C': None,
}, '''\
1: error: macro definition A refers to itself
3: note: evaluated from C
2: note: evaluated from B
''')

check_macro_eval('''\
#define A 1 +
''', {
    'A': None,
}, '''\
1: error: uninterpretable macro token sequence: 1 +
''')

check_macro_eval('''\
#define A 3*5
''', {
    'A': None,
}, '''\
1: error: uninterpretable macro token sequence: 3 * 5
''')

check_macro_eval('''\
#define A 3 + 5
''', {
    'A': 8,
}, '''\
1: error: missing parentheses around + expression
1: note: in definition of macro A
''')

if errors:
    sys.exit(1)
