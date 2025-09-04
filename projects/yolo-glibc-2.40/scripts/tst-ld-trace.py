#!/usr/bin/python3
# Dump the output of LD_TRACE_LOADED_OBJECTS in architecture neutral format.
# Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
import os
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

def is_vdso(lib):
    return lib.startswith('linux-gate') or lib.startswith('linux-vdso')


def parse_trace(cmd, fref):
    new_env = os.environ.copy()
    new_env['LD_TRACE_LOADED_OBJECTS'] = '1'
    trace_out = subprocess.run(cmd, stdout=subprocess.PIPE, check=True,
                               universal_newlines=True, env=new_env).stdout
    trace = []
    for line in trace_out.splitlines():
        line = line.strip()
        if is_vdso(line):
            continue
        fields = line.split('=>' if '=>' in line else ' ')
        lib = os.path.basename(fields[0].strip())
        if lib.startswith('ld'):
            lib = 'ld'
        elif lib.startswith('libc'):
            lib = 'libc'
        found = 1 if fields[1].strip() != 'not found' else 0
        trace += ['{} {}'.format(lib, found)]
    trace = sorted(trace)

    reference = sorted(line.replace('\n','') for line in fref.readlines())

    ret = 0 if trace == reference else 1
    if ret != 0:
        for i in reference:
            if i not in trace:
                print("Only in {}: {}".format(fref.name, i))
        for i in trace:
            if i not in reference:
                print("Only in trace: {}".format(i))

    sys.exit(ret)


def get_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',
                        help='comand to run')
    parser.add_argument('reference',
                        help='reference file to compare')
    return parser


def main(argv):
    parser = get_parser()
    opts = parser.parse_args(argv)
    with open(opts.reference, 'r') as fref:
        # Remove the initial 'env' command.
        parse_trace(opts.command.split()[1:], fref)


if __name__ == '__main__':
    main(sys.argv[1:])
