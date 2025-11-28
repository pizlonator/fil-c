#!/usr/bin/env python3

# Copyright 2021 Simon McVittie
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

import unittest
import sys

class TestIntrospection(unittest.TestCase):
    def setUp(self):
        try:
            import gi
        except ImportError:
            self.skipTest('gi module not available')

        gi.require_version('Graphene', '1.0')

    def test_basics(self):
        '''Assert that introspection basically works'''
        from gi.repository import Graphene

        self.assertIsNotNone(Graphene.Box.empty())

    def test_simd_not_exposed(self):
        '''Assert that SIMD implementation details are not present'''
        from gi.repository import Graphene

        for name in (
            'HAS_ARM_NEON',
            'HAS_GCC',
            'HAS_SCALAR',
            'HAS_SSE',
            'SIMD_S',
            'USE_ARM_NEON',
            'USE_GCC',
            'USE_SCALAR',
            'USE_SSE',
        ):
            with self.assertRaises(
                AttributeError,
                msg='%s should not be defined' % name,
            ):
                getattr(Graphene, name)

def main():
    try:
        from tap.runner import TAPTestRunner
    except ImportError:
        # Minimal TAP implementation
        print('1..1')
        program = unittest.main(exit=False)
        if program.result.wasSuccessful():
            print('ok 1 - %r' % program.result)
        else:
            print('not ok 1 - %r' % program.result)
    else:
        runner = TAPTestRunner()
        runner.set_stream(True)
        unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()
