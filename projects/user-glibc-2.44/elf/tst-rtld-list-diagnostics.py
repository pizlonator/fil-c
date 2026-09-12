#!/usr/bin/python3
# Test that the ld.so --list-diagnostics output has the expected syntax.
# Copyright (C) 2022-2026 Free Software Foundation, Inc.
# Copyright The GNU Toolchain Authors.
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

import argparse
import collections
import subprocess
import sys

try:
    subprocess.run
except:
    class _CompletedProcess:
        def __init__(self, args, returncode, stdout=None, stderr=None):
            self.args = args
            self.returncode = returncode
            self.stdout = stdout
            self.stderr = stderr

    def _run(*popenargs, input=None, timeout=None, check=False, **kwargs):
        assert(timeout is None)
        with subprocess.Popen(*popenargs, **kwargs) as process:
            try:
                stdout, stderr = process.communicate(input)
            except:
                process.kill()
                process.wait()
                raise
            returncode = process.poll()
            if check and returncode:
                raise subprocess.CalledProcessError(returncode, popenargs)
        return _CompletedProcess(popenargs, returncode, stdout, stderr)

    subprocess.run = _run

# Number of errors encountered.  Zero means no errors (test passes).
errors = 0

def parse_line(line):
    """Parse a line of --list-diagnostics output.

    This function returns a pair (SUBSCRIPTS, VALUE).  VALUE is either
    a byte string or an integer.  SUBSCRIPT is a tuple of (LABEL,
    INDEX) pairs, where LABEL is a field identifier (a string), and
    INDEX is an integer or None, to indicate that this field is not
    indexed.

    """

    # Extract the list of subscripts before the value.
    idx = 0
    subscripts = []
    while line[idx] != '=':
        start_idx = idx

        # Extract the label.
        while line[idx] not in '[.=':
            idx += 1
        label = line[start_idx:idx]

        if line[idx] == '[':
            # Subscript with a 0x index.
            assert label
            close_bracket = line.index(']', idx)
            index = line[idx + 1:close_bracket]
            assert index.startswith('0x')
            index = int(index, 0)
            subscripts.append((label, index))
            idx = close_bracket + 1
        else: # '.' or '='.
            if label:
                subscripts.append((label, None))
            if line[idx] == '.':
                idx += 1

    # The value is either a string or a 0x number.
    value = line[idx + 1:]
    if value[0] == '"':
        # Decode the escaped string into a byte string.
        assert value[-1] == '"'
        idx = 1
        result = []
        while True:
            ch = value[idx]
            if ch == '\\':
                if value[idx + 1] in '"\\':
                    result.append(ord(value[idx + 1]))
                    idx += 2
                else:
                    result.append(int(value[idx + 1:idx + 4], 8))
                    idx += 4
            elif ch == '"':
                assert idx == len(value) - 1
                break
            else:
                result.append(ord(value[idx]))
                idx += 1
        value = bytes(result)
    else:
        # Convert the value into an integer.
        assert value.startswith('0x')
        value = int(value, 0)
    return (tuple(subscripts), value)

assert parse_line('a.b[0x1]=0x2') == ((('a', None), ('b', 1)), 2)
assert parse_line(r'b[0x3]="four\040\"\\"') == ((('b', 3),), b'four \"\\')

# ABNF for a line of --list-diagnostics output.
diagnostics_abnf = r"""
HEXDIG = %x30-39 / %x61-6f ; lowercase a-f only
ALPHA = %x41-5a / %x61-7a / %x7f ; letters and underscore
ALPHA-NUMERIC = ALPHA / %x30-39 / "_"
DQUOTE = %x22 ; "

; Numbers are always hexadecimal and use a 0x prefix.
hex-value-prefix = %x30 %x78
hex-value = hex-value-prefix 1*HEXDIG

; Strings use octal escape sequences and \\, \".
string-char = %x20-21 / %x23-5c / %x5d-7e ; printable but not "\
string-quoted-octal = %x30-33 2*2%x30-37
string-quoted = "\" ("\" / DQUOTE / string-quoted-octal)
string-value = DQUOTE *(string-char / string-quoted) DQUOTE

value = hex-value / string-value

label = ALPHA *ALPHA-NUMERIC
index = "[" hex-value "]"
subscript = label [index]

line = subscript *("." subscript) "=" value
"""

def check_consistency_with_manual(manual_path):
    """Verify that the code fragments in the manual match this script.

    The code fragments are duplicated to clarify the dual license.
    """

    global errors

    def extract_lines(path, start_line, end_line, skip_lines=()):
        result = []
        with open(path) as inp:
            capturing = False
            for line in inp:
                if line.strip() == start_line:
                    capturing = True
                elif not capturing or line.strip() in skip_lines:
                    continue
                elif line.strip() == end_line:
                    capturing = False
                else:
                    result.append(line)
        if not result:
            raise ValueError('{!r} not found in {!r}'.format(start_line, path))
        if capturing:
            raise ValueError('{!r} not found in {!r}'.format(end_line, path))
        return result

    def check(name, manual, script):
        global errors

        if manual == script:
            return
        print('error: {} fragment in manual is different'.format(name))
        import difflib
        sys.stdout.writelines(difflib.unified_diff(
            manual, script, fromfile='manual', tofile='script'))
        errors += 1

    manual_abnf = extract_lines(manual_path,
                                '@c ABNF-START', '@end smallexample',
                                skip_lines=('@smallexample',))
    check('ABNF', diagnostics_abnf.splitlines(keepends=True)[1:], manual_abnf)

# If the abnf module can be imported, run an additional check that the
# 'line' production from the ABNF grammar matches --list-diagnostics
# output lines.
try:
    import abnf
except ImportError:
    abnf = None
    print('info: skipping ABNF validation because the abnf module is missing')

if abnf is not None:
    class Grammar(abnf.Rule):
        pass

    Grammar.load_grammar(diagnostics_abnf)

    def parse_abnf(line):
        global errors

        # Just verify that the line parses.
        try:
            Grammar('line').parse_all(line)
        except abnf.ParseError:
            print('error: ABNF parse error:', repr(line))
            errors += 1
else:
    def parse_abnf(line):
        pass


def parse_diagnostics(cmd):
    global errors
    diag_out = subprocess.run(cmd, stdout=subprocess.PIPE, check=True,
                              universal_newlines=True, shell=True).stdout
    if diag_out[-1] != '\n':
        print('error: ld.so output does not end in newline')
        errors += 1

    PathType = collections.namedtuple('PathType',
                                      'has_index value_type original_line')
    # Mapping tuples of labels to PathType values.
    path_types = {}

    seen_subscripts = {}

    for line in diag_out.splitlines():
        parse_abnf(line)
        subscripts, value = parse_line(line)

        # Check for duplicates.
        if subscripts in seen_subscripts:
            print('error: duplicate value assignment:', repr(line))
            print('  previous line:,', repr(seen_subscripts[line]))
            errors += 1
        else:
            seen_subscripts[subscripts] = line

        # Compare types against the previously seen labels.
        labels = tuple([label for label, index in subscripts])
        has_index = tuple([index is not None for label, index in subscripts])
        value_type = type(value)
        if labels in path_types:
            previous_type = path_types[labels]
            if has_index != previous_type.has_index:
                print('error: line has mismatch of indexing:', repr(line))
                print('  index types:', has_index)
                print('  previous:   ', previous_type.has_index)
                print('  previous line:', repr(previous_type.original_line))
                errors += 1
            if value_type != previous_type.value_type:
                print('error: line has mismatch of value type:', repr(line))
                print('  value type:', value_type.__name__)
                print('  previous:  ', previous_type.value_type.__name__)
                print('  previous line:', repr(previous_type.original_line))
                errors += 1
        else:
            path_types[labels] = PathType(has_index, value_type, line)

        # Check that this line does not add indexing to a previous value.
        for idx in range(1, len(subscripts) - 1):
            if subscripts[:idx] in path_types:
                print('error: line assigns to atomic value:', repr(line))
                print('  previous line:', repr(previous_type.original_line))
                errors += 1

    if errors:
        sys.exit(1)

def get_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manual',
                        help='path to .texi file for consistency checks')
    parser.add_argument('command',
                        help='comand to run')
    return parser


def main(argv):
    parser = get_parser()
    opts = parser.parse_args(argv)

    if opts.manual:
        check_consistency_with_manual(opts.manual)

    parse_diagnostics(opts.command)

    if errors:
        sys.exit(1)

if __name__ == '__main__':
    main(sys.argv[1:])
