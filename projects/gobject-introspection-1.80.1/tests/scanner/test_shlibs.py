import unittest
import sys
import os

from giscanner.shlibs import resolve_from_ldd_output, sanitize_shlib_path


class TestLddParser(unittest.TestCase):

    def test_resolve_from_ldd_output(self):
        output = '''\
            libglib-2.0.so.0 => /usr/lib/x86_64-linux-gnu/libglib-2.0.so.0 (0x00007fbe12d68000)
            libgtk-3.so.0 => /usr/lib/x86_64-linux-gnu/libgtk-3.so.0 (0x00007fbe12462000)
            libgdk-3.so.0 => /usr/lib/x86_64-linux-gnu/libgdk-3.so.0 (0x00007fbe1216c000)
            libpango-1.0.so.0 => /usr/lib/x86_64-linux-gnu/libpango-1.0.so.0 (0x00007fbe11d1a000)
            libatk-1.0.so.0 => /usr/lib/x86_64-linux-gnu/libatk-1.0.so.0 (0x00007fbe11af4000)'''
        libraries = ['glib-2.0', 'gtk-3', 'pango-1.0']

        self.assertEqual(
            ['libglib-2.0.so.0', 'libgtk-3.so.0', 'libpango-1.0.so.0'],
            resolve_from_ldd_output(libraries, output))

    @unittest.skipUnless(os.name == "posix", "posix only")
    def test_resolve_from_ldd_output_macos_rpath(self):
        output = '''\
            @rpath/libbarapp-1.0.dylib (compatibility version 0.0.0, current version 0.0.0)
            /foo/libgio-2.0.0.dylib (compatibility version 5801.0.0, current version 5801.3.0)
            /foo/libgmodule-2.0.0.dylib (compatibility version 5801.0.0, current version 5801.3.0)'''

        libraries = ['barapp-1.0']
        shlibs = resolve_from_ldd_output(libraries, output)
        self.assertEqual(shlibs, ['@rpath/libbarapp-1.0.dylib'])
        self.assertEqual(sanitize_shlib_path(shlibs[0]), 'libbarapp-1.0.dylib')

    @unittest.skipUnless(os.name == "posix", "posix only")
    def test_sanitize_shlib_path(self):
        self.assertEqual(
            sanitize_shlib_path('@rpath/libbarapp-1.0.dylib'),
            'libbarapp-1.0.dylib')

        self.assertEqual(
            sanitize_shlib_path('/foo/bar'),
            '/foo/bar' if sys.platform == 'darwin' else 'bar')

    def test_unresolved_library(self):
        output = ''
        libraries = ['foo']

        with self.assertRaises(SystemExit, msg='can\'t resolve libraries to shared libraries: foo'):
            resolve_from_ldd_output(libraries, output)

    def test_prefixed_library_name(self):
        output = '''\
           /usr/lib/liblibX.so
           /usr/lib/libX.so'''

        self.assertEqual(
            ['/usr/lib/libX.so'],
            resolve_from_ldd_output(['X'], output))
        self.assertEqual(
            ['/usr/lib/liblibX.so'],
            resolve_from_ldd_output(['libX'], output))

    def test_suffixed_library_name(self):
        output = '''\
            libpangocairo.so.0 => /usr/lib/x86_64-linux-gnu/libpangocairo.so.0 (0x00)
            libpangoft2.so.0 => /usr/lib/x86_64-linux-gnu/libpangoft2.so.0 (0x00)
            libpango.so.0 => /usr/lib/x86_64-linux-gnu/libpango.so.0 (0x00)'''
        libraries = ['pango']

        self.assertEqual(
            ['libpango.so.0'],
            resolve_from_ldd_output(libraries, output))

    def test_header_is_ignored(self):
        output = '''/tmp-introspection/libfoo.so.999:
            0000000000000000 0000000000000000 rlib  0    3   0      /usr/local/lib/libfoo.so.1'''
        libraries = ['foo']

        self.assertEqual(
            ['/usr/local/lib/libfoo.so.1'],
            resolve_from_ldd_output(libraries, output))

    def test_executable_path_includes_library_name(self):
        """
        Regression test for https://gitlab.gnome.org/GNOME/gobject-introspection/issues/208
        """
        output = '''/usr/ports/pobj/libgepub-0.6.0/build-amd64/tmp-introspectnxmyodg1/Gepub-0.6:
            Start            End              Type  Open Ref GrpRef Name
            00001066c8400000 00001066c8605000 exe   2    0   0      /usr/ports/pobj/libgepub-0.6.0/build-amd64/tmp-introspectnxmyodg1/Gepub-0.6
            000010690019c000 00001069003a8000 rlib  0    1   0      /usr/local/lib/libgepub-0.6.so.0.0'''
        libraries = ['gepub-0.6']

        self.assertEqual(
            ['/usr/local/lib/libgepub-0.6.so.0.0'],
            resolve_from_ldd_output(libraries, output))

    def test_library_path_includes_library_name(self):
        output = '''/usr/ports/pobj/gnome-music-3.28.1/build-amd64/tmp-introspectuz5xaun3/Gd-1.0:
            Start            End              Type  Open Ref GrpRef Name
            0000070e40f00000 0000070e41105000 exe   2    0   0      /usr/ports/pobj/gnome-music-3.28.1/build-amd64/tmp-introspectuz5xaun3/Gd-1.0
            00000710f9b39000 00000710f9d51000 rlib  0    1   0      /usr/ports/pobj/gnome-music-3.28.1/build-amd64/subprojects/libgd/libgd/libgd.so'''
        libraries = ['gd']

        self.assertEqual(
            ['/usr/ports/pobj/gnome-music-3.28.1/build-amd64/subprojects/libgd/libgd/libgd.so'],
            resolve_from_ldd_output(libraries, output))

    def test_basename(self):
        output = '''/usr/lib/libfoo.so'''

        self.assertEqual(
            ['/usr/lib/libfoo.so'],
            resolve_from_ldd_output(['foo'], output))


if __name__ == '__main__':
    unittest.main()
