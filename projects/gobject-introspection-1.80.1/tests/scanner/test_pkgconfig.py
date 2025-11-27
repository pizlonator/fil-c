# -*- coding: UTF-8 -*-
#
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
#

import contextlib
import os
import sys
import tempfile
import textwrap
import unittest

from giscanner import pkgconfig


@contextlib.contextmanager
def pkg_config_script(code):
    """Points PKG_CONFIG environment variable to an executable file with given python code.
    Common leading whitespace is removed from code for convenience."""

    with tempfile.NamedTemporaryFile(mode='wb', suffix='.py', delete=False) as file:
        file.write('#!{}\n'.format(sys.executable).encode("utf-8"))
        file.write(textwrap.dedent(code).encode("utf-8"))
        os.chmod(file.name, 0o777)

    try:
        yield [sys.executable, file.name]
    finally:
        os.unlink(file.name)


class PkgConfigTest(unittest.TestCase):

    def test_non_zero_exit_code(self):
        """Checks that non-zero exit code from pkg-config results in exception."""
        s = """
        import sys
        sys.exit(1)
        """
        with self.assertRaises(pkgconfig.PkgConfigError):
            with pkg_config_script(s) as command:
                pkgconfig.cflags(['foo'], command=command, ignore_errors=False)

    def test_cflags(self):
        """Checks arguments passed to pkg-config when asking for --cflags."""
        s = """
        import sys
        args = sys.argv[1:]
        assert len(args) == 4
        assert args[0] == '--cflags'
        assert args[1] == 'foo-1.0'
        assert args[2] == 'bar-2.0'
        assert args[3] == 'baz'
        """
        with pkg_config_script(s) as command:
            pkgconfig.cflags(['foo-1.0', 'bar-2.0', 'baz'], command=command)

    def test_libs(self):
        """Checks arguments passed to pkg-config when asking for --libs."""
        s = """
        import sys
        args = sys.argv[1:]
        assert len(args) == 3
        assert args[0] == '--libs'
        assert args[1] == 'a'
        assert args[2] == 'b-42.0'
        """
        with pkg_config_script(s) as command:
            pkgconfig.libs(['a', 'b-42.0'], command=command)

    @unittest.skipIf(os.name == "nt", "FIXME")
    def test_non_ascii_output(self):
        with pkg_config_script("""print("-L'zażółć gęślą jaźń'")""") as command:
            flags = pkgconfig.cflags(['test-1.0'], command=command)
            self.assertEqual(flags, ["-Lzażółć gęślą jaźń"])

        with pkg_config_script("""print('-Lé')""") as command:
            flags = pkgconfig.cflags(['test-1.0'], command=command)
            self.assertEqual(flags, ['-Lé'])

    def test_shell_word_splitting_rules(self):
        # Regression test for issue #171.
        with pkg_config_script("""print('-L"/usr/lib64" -lgit2')""") as command:
            flags = pkgconfig.cflags(['foo-2.0'], command=command)
            self.assertEqual(flags, ['-L/usr/lib64', '-lgit2'])

        # Macro define for a C string literal.
        with pkg_config_script('''print("""-DLOG='"HELLO"'""")''') as command:
            flags = pkgconfig.cflags(['bar-3.0'], command=command)
            self.assertEqual(flags, ['-DLOG="HELLO"'])


if __name__ == '__main__':
    unittest.main()
