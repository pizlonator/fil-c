#!/usr/bin/env python3

# Copyright 2017  Emmanuele Bassi
#
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import sys
import os
import argparse

def write_template(filename, data):
    with open(filename, 'w') as f:
        f.write(data)

def build_template(testdir, testname):
    return """[Test]
Type=session
Exec={}
TestEnvironment=MUTEST_OUTPUT=tap;
""".format(os.path.join(testdir, testname))

argparser = argparse.ArgumentParser(description='Generate installed-test data.')
argparser.add_argument('--testdir', metavar='dir', required=True, help='Installed test directory')
argparser.add_argument('--testname', metavar='name', required=True, help='Installed test name')
argparser.add_argument('--outfile', metavar='file', required=True, help='Output file')
argparser.add_argument('--outdir', metavar='dir', required=True, help='Output directory')
args = argparser.parse_args()

write_template(os.path.join(args.outdir, args.outfile), build_template(args.testdir, args.testname))
