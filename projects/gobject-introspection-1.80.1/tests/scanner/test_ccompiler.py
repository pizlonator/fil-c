# GObject-Introspection - a framework for introspecting GObject libraries
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

import distutils
import os
import shlex
import unittest
from contextlib import contextmanager

from giscanner.ccompiler import CCompiler, FLAGS_RETAINING_MACROS


@contextmanager
def Environ(new_environ):
    """Context manager for os.environ."""
    old_environ = os.environ.copy()
    os.environ.clear()
    os.environ.update(new_environ)
    try:
        yield
    finally:
        # Restore previous environment
        os.environ.clear()
        os.environ.update(old_environ)


@unittest.skipIf(os.name != 'posix', 'tests for unix compiler only')
class UnixCCompilerTest(unittest.TestCase):

    def assertListStartsWith(self, seq, prefix):
        """Checks whether seq starts with specified prefix."""
        if not isinstance(seq, list):
            raise self.fail('First argument is not a list: %r' % (seq,))
        if not isinstance(prefix, list):
            raise self.fail('Second argument is not a list: %r' % (prefix,))
        self.assertSequenceEqual(seq[:len(prefix)], prefix, seq_type=list)

    def assertIsSubsequence(self, list1, list2):
        """Checks whether list1 is a subsequence of list2. Not necessarily a contiguous one."""
        if not isinstance(list1, list):
            raise self.fail('First argument is not a list: %r' % (list1,))
        if not isinstance(list2, list):
            raise self.fail('Second argument is not a list: %r' % (list2,))
        start = 0
        for elem in list1:
            try:
                start = list2.index(elem, start) + 1
            except ValueError:
                self.fail('%r is not a subsequence of %r' % (list1, list2))

    def test_link_cmd(self):
        with Environ(dict(CC="foobar")):
            compiler = CCompiler()
            self.assertEqual(compiler.linker_cmd[0], "foobar")

    def test_link_args_override(self):
        with Environ(dict(CC="foobar")):
            compiler = CCompiler()
            self.assertEqual(compiler.compiler.linker_so[0], "foobar")

    def compile_args(self, environ={}, compiler_name='unix',
                     pkg_config_cflags=[], cpp_includes=[],
                     source='a.c', init_sections=[]):
        """Returns a list of arguments that would be passed to the compiler executing given compilation step."""

        try:
            from unittest.mock import patch
        except ImportError as e:
            raise unittest.SkipTest(e)

        with patch.object(distutils.ccompiler.CCompiler, 'spawn') as spawn:
            with Environ(environ):
                cc = CCompiler(compiler_name=compiler_name)
                # Avoid check if target is newer from source.
                cc.compiler.force = True
                # Don't actually do anything.
                cc.compiler.dry_run = True
                cc.compile(pkg_config_cflags, cpp_includes, [source], init_sections)
        self.assertEqual(1, spawn.call_count)
        args, kwargs = spawn.call_args
        return args[0]

    def preprocess_args(self, environ={}, compiler_name=None,
                        source='a.c', output=None, cpp_options=[]):
        """Returns a list of arguments that would be passed to the preprocessor executing given preprocessing step."""

        try:
            from unittest.mock import patch
        except ImportError as e:
            raise unittest.SkipTest(e)

        with patch.object(distutils.ccompiler.CCompiler, 'spawn') as spawn:
            with Environ(environ):
                cc = CCompiler(compiler_name=compiler_name)
                # Avoid check if target is newer from source.
                cc.compiler.force = True
                # Don't actually do anything.
                cc.compiler.dry_run = True
                cc.preprocess(source, output, cpp_options)
        self.assertEqual(1, spawn.call_count)
        args, kwargs = spawn.call_args
        return args[0]

    @unittest.skip("Currently a Python build time compiler is used as the default.")
    def test_compile_default(self):
        """Checks that cc is used as the default compiler."""
        args = self.compile_args()
        self.assertListStartsWith(args, ['cc'])

    def test_compile_cc(self):
        """Checks that CC overrides used compiler."""
        args = self.compile_args(environ=dict(CC='supercc'))
        self.assertListStartsWith(args, ['supercc'])

    def test_preprocess_cc(self):
        """Checks that CC overrides used preprocessor when CPP is unspecified."""
        args = self.preprocess_args(environ=dict(CC='clang'))
        self.assertListStartsWith(args, ['clang'])
        self.assertIn('-E', args)

    def test_preprocess_cpp(self):
        """Checks that CPP overrides used preprocessor regardless of CC."""
        args = self.preprocess_args(environ=dict(CC='my-compiler', CPP='my-preprocessor'))
        self.assertListStartsWith(args, ['my-preprocessor'])
        self.assertNotIn('-E', args)

    @unittest.skip("Currently a Python build time preprocessor is used as the default")
    def test_preprocess_default(self):
        """Checks that cpp is used as the default preprocessor."""
        args = self.preprocess_args()
        self.assertListStartsWith(args, ['cpp'])

    def test_multiple_args_in_cc(self):
        """Checks that shell word splitting rules are used to split CC."""
        args = self.compile_args(environ=dict(CC='build-log -m " hello  there  " gcc'))
        self.assertListStartsWith(args, ['build-log', '-m', ' hello  there  ', 'gcc'])

    def test_multiple_args_in_cpp(self):
        """Checks that shell word splitting rules are used to split CPP."""
        args = self.preprocess_args(environ=dict(CPP='build-log -m " hello  there" gcc -E'))
        self.assertListStartsWith(args, ['build-log', '-m', ' hello  there', 'gcc', '-E'])

    def test_deprecation_warnings_are_disabled_during_compilation(self):
        """Checks that deprecation warnings are disabled during compilation."""
        args = self.compile_args()
        self.assertIn('-Wno-deprecated-declarations', args)

    def test_preprocess_includes_cppflags(self):
        """Checks that preprocessing step includes CPPFLAGS."""
        args = self.preprocess_args(environ=dict(CPPFLAGS='-fsecure -Ddebug'))
        self.assertIsSubsequence(['-fsecure', '-Ddebug'], args)

    def test_compile_includes_cppflags(self):
        """Checks that compilation step includes both CFLAGS and CPPFLAGS, in that order."""
        args = self.compile_args(environ=dict(CFLAGS='-lfoo -Da="x y" -Weverything',
                                              CPPFLAGS='-fsecure -Ddebug'))
        self.assertIsSubsequence(['-lfoo', '-Da=x y', '-Weverything', '-fsecure', '-Ddebug'],
                                 args)

    def test_flags_retaining_macros_are_filtered_out(self):
        """Checks that flags that would retain macros after preprocessing step are filtered out."""
        args = self.preprocess_args(cpp_options=list(FLAGS_RETAINING_MACROS))
        for flag in FLAGS_RETAINING_MACROS:
            self.assertNotIn(flag, args)

    @unittest.expectedFailure
    def test_macros(self):
        """"Checks that macros from CFLAGS are defined only once."""
        args = self.compile_args(environ=dict(CFLAGS='-DSECRET_MACRO'))
        self.assertEqual(1, args.count('-DSECRET_MACRO'))

    @unittest.expectedFailure
    def test_flags_retaining_macros_are_filtered_out_from_cppflags(self):
        """Checks that flags that would retain macros after preprocessing step are filtered out from CPPFLAGS."""
        cppflags = ' '.join(shlex.quote(flag) for flag in FLAGS_RETAINING_MACROS)
        args = self.preprocess_args(environ=dict(CPPFLAGS=cppflags))
        for flag in FLAGS_RETAINING_MACROS:
            self.assertNotIn(flag, args)

    def test_preprocess_preserves_comments(self):
        """Checks that preprocessing step includes flag that preserves comments."""
        args = self.preprocess_args()
        self.assertIn('-C', args)

    def test_perprocess_includes_cwd(self):
        """Checks that preprocessing includes current working directory."""
        args = self.preprocess_args()
        self.assertIn('-I.', args)

    def test_preprocess_command(self):
        """"Checks complete preprocessing command."""
        args = self.preprocess_args(environ=dict(CPP='gcc -E'),
                                    source='/tmp/file.c')
        self.assertEqual(['gcc', '-E', '-I.', '-C', '/tmp/file.c'],
                         args)

    def test_compile_command(self):
        """Checks complete compilation command."""
        args = self.compile_args(environ=dict(CC='clang'),
                                 source='/tmp/file.c')
        self.assertEqual(['clang',
                          '-c', '/tmp/file.c',
                          '-o', '/tmp/file.o',
                          '-Wno-deprecated-declarations'],
                         args)


if __name__ == '__main__':
    unittest.main()
