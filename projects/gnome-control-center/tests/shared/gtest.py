#!/usr/bin/env python3
# Copyright © 2018 Red Hat, Inc
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#
# Authors: Benjamin Berg <bberg@redhat.com>

import os
import sys
import subprocess
import functools

class _GTestSingleProp(object):
    """Property which creates a bound method for calling the specified test."""
    def __init__(self, test):
        self.test = test

    @staticmethod
    def __func(self, test):
        self._gtest_single(test)

    def __get__(self, obj, cls):
        bound_method = self.__func.__get__(obj, cls)
        partial_method = functools.partial(bound_method, self.test)
        partial_method.__doc__ = bound_method.__doc__
        # Set a qualified name using the qualified name of the class and
        # function. Note that this is different from the generated attribute
        # name as it is missing the test_%03d_ prefix.
        partial_method.__qualname__ = '%s.%s' % (cls.__qualname__, self.test)

        return partial_method


class _GTestMeta(type):
    def __new__(cls, name, bases, namespace, **kwds):
        result = type.__new__(cls, name, bases, dict(namespace))

        if result.g_test_exe is not None:
            try:
                _GTestMeta.make_tests(result.g_test_exe, result)
            except Exception as e:
                print('')
                print(e)
                print('Error generating separate test funcs, will call binary once.')
                result.test_all = result._gtest_all

        return result

    @staticmethod
    def make_tests(exe, result):
        env = os.environ.copy()
        env['G_MESSAGES_DEBUG'] = ''
        test = subprocess.Popen([exe, '-l'], stdout=subprocess.PIPE, stderr=None, env=env)
        stdout, stderr = test.communicate()

        if test.returncode != 0:
            raise AssertionError('Execution of GTest executable to query the tests returned non-zero exit code!')

        stdout = stdout.decode('utf-8')

        for i, test in enumerate(stdout.split('\n')):
            if not test:
                continue

            # Number it and make sure the function name is prefixed with 'test'.
            # Keep the rest as is, we don't care about the fact that the function
            # names cannot be typed in.
            name = 'test_%03d_' % (i + 1) + test
            setattr(result, name, _GTestSingleProp(test))


class GTest(metaclass = _GTestMeta):
    """Helper class to run GLib test. A test function will be created for each
    test from the executable.

    Use by using this class as a mixin and setting g_test_exe to an appropriate
    value.
    """

    #: The GTest based executable
    g_test_exe = None
    #: Timeout when running a single test
    g_test_single_timeout = None
    #: Timeout when running all tests in one go
    g_test_all_timeout = None

    def _gtest_single(self, test):
        assert(test)
        p = subprocess.Popen([self.g_test_exe, '-q', '-p', test], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            stdout, stderr = p.communicate(timeout=self.g_test_single_timeout)
        except subprocess.TimeoutExpired:
            p.kill()
            stdout, stderr = p.communicate()
            stdout += b'\n\nTest was aborted due to timeout'

        try:
            stdout = stdout.decode('utf-8')
        except UnicodeDecodeError:
            pass

        if p.returncode != 0:
            self.fail(stdout)

    def _gtest_all(self):
        subprocess.check_call([self.g_test_exe], timeout=self.g_test_all_timeout)
