# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008 Colin Walters
# Copyright (C) 2008 Johan Dahlin
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

import os
import sys
import shlex
import subprocess
import tempfile

from .gdumpparser import IntrospectionBinary
from . import pkgconfig, utils
from .ccompiler import CCompiler

# bugzilla.gnome.org/558436
# Compile a binary program which is then linked to a library
# we want to introspect, in order to call its get_type functions.

_PROGRAM_TEMPLATE = """/* This file is generated, do not edit */

#undef GLIB_VERSION_MIN_REQUIRED
#undef GLIB_VERSION_MAX_ALLOWED

#include <glib.h>
#include <string.h>
#include <stdlib.h>

%(gdump_include)s

int
main(int argc, char **argv)
{
  GError *error = NULL;
  const char *introspect_dump_prefix = "--introspect-dump=";

#if !GLIB_CHECK_VERSION(2,35,0)
  g_type_init ();
#endif

  %(init_sections)s

  if (argc != 2 || !g_str_has_prefix (argv[1], introspect_dump_prefix))
    {
      g_printerr ("Usage: %%s --introspect-dump=input,output", argv[0]);
      exit (1);
    }

  if (!dump_irepository (argv[1] + strlen(introspect_dump_prefix), &error))
    {
      g_printerr ("%%s\\n", error->message);
      exit (1);
    }
  exit (0);
}
"""


class CompilerError(Exception):
    pass


class LinkerError(Exception):
    pass


class DumpCompiler(object):

    _compiler = None

    def __init__(self, options, get_type_functions, error_quark_functions):
        self._options = options
        self._get_type_functions = get_type_functions
        self._error_quark_functions = error_quark_functions

        # Acquire the compiler (and linker) commands via the CCompiler class in ccompiler.py
        self._compiler = CCompiler()

        self._uninst_srcdir = os.environ.get('UNINSTALLED_INTROSPECTION_SRCDIR')
        self._packages = ['gio-2.0', 'gmodule-2.0']
        self._packages.extend(options.packages)

    # Public API

    def run(self):
        # We have to use the current directory to work around Unix
        # sysadmins who mount /tmp noexec
        tmpdir = tempfile.mkdtemp('', 'tmp-introspect', dir=os.getcwd())
        os.mkdir(os.path.join(tmpdir, '.libs'))

        tpl_args = {}
        if self._uninst_srcdir is not None:
            gdump_path = os.path.join(self._uninst_srcdir, 'giscanner', 'gdump.c')
        else:
            try:
                gdump_path = GDUMP_PATH
            except NameError:
                gdump_path = os.path.join(os.path.join(DATADIR),
                                          'gobject-introspection-1.0', 'gdump.c')
        if not os.path.isfile(gdump_path):
            raise SystemExit("Couldn't find %r" % (gdump_path, ))
        with open(gdump_path, encoding='utf-8') as gdump_file:
            gdump_contents = gdump_file.read()
        tpl_args['gdump_include'] = gdump_contents
        tpl_args['init_sections'] = "\n".join(self._options.init_sections)

        c_path = self._generate_tempfile(tmpdir, '.c')
        with open(c_path, 'w', encoding='utf-8') as f:
            f.write(_PROGRAM_TEMPLATE % tpl_args)

            # We need to reference our get_type and error_quark functions
            # to make sure they are pulled in at the linking stage if the
            # library is a static library rather than a shared library.
            if len(self._get_type_functions) > 0:
                for func in self._get_type_functions:
                    f.write("extern GType " + func + "(void);\n")
                f.write("G_MODULE_EXPORT GType (*GI_GET_TYPE_FUNCS_[])(void) = {\n")
                first = True
                for func in self._get_type_functions:
                    if first:
                        first = False
                    else:
                        f.write(",\n")
                    f.write("  " + func)
                f.write("\n};\n")
            if len(self._error_quark_functions) > 0:
                for func in self._error_quark_functions:
                    f.write("extern GQuark " + func + "(void);\n")
                f.write("G_MODULE_EXPORT GQuark (*GI_ERROR_QUARK_FUNCS_[])(void) = {\n")
                first = True
                for func in self._error_quark_functions:
                    if first:
                        first = False
                    else:
                        f.write(",\n")
                    f.write("  " + func)
                f.write("\n};\n")

        if self._compiler.compiler.exe_extension:
            ext = self._compiler.compiler.exe_extension
        else:
            ext = ''

        bin_path = self._generate_tempfile(tmpdir, ext)

        try:
            introspection_obj = self._compile(c_path)
        except CompilerError as e:
            if not utils.have_debug_flag('save-temps'):
                utils.rmtree(tmpdir)
            raise SystemExit('compilation of temporary binary failed:' + str(e))

        try:
            self._link(bin_path, introspection_obj)
        except LinkerError as e:
            if not utils.have_debug_flag('save-temps'):
                utils.rmtree(tmpdir)
            raise SystemExit('linking of temporary binary failed: ' + str(e))

        return IntrospectionBinary([bin_path], tmpdir)

    # Private API

    def _generate_tempfile(self, tmpdir, suffix=''):
        tmpl = '%s-%s%s' % (self._options.namespace_name,
                            self._options.namespace_version, suffix)
        return os.path.join(tmpdir, tmpl)

    def _compile(self, *sources):
        cflags = pkgconfig.cflags(self._packages,
                                  msvc_syntax=self._compiler.check_is_msvc())
        cflags.extend(self._options.cflags)
        return self._compiler.compile(cflags,
                                      self._options.cpp_includes,
                                      sources,
                                      self._options.init_sections)

    def _link(self, output, sources):
        args = []
        libtool = utils.get_libtool_command(self._options)
        if libtool:
            # Note: MSVC Builds do not use libtool!
            args.extend(libtool)
            args.append('--mode=link')
            args.append('--tag=CC')
            if self._options.quiet:
                args.append('--silent')

        args.extend(self._compiler.linker_cmd)

        # We can use -o for the Microsoft compiler/linker,
        # but it is considered deprecated usage
        if self._compiler.check_is_msvc():
            args.extend(['-out:' + output])
        else:
            args.extend(['-o', output])
            if libtool:
                if utils.host_os() == 'nt':
                    args.append('-Wl,--export-all-symbols')
                else:
                    args.append('-export-dynamic')

        if not self._compiler.check_is_msvc():
            # These envvars are not used for MSVC Builds!
            # MSVC Builds use the INCLUDE, LIB envvars,
            # which are automatically picked up during
            # compilation and linking
            for cppflag in shlex.split(os.environ.get('CPPFLAGS', '')):
                args.append(cppflag)
            for cflag in shlex.split(os.environ.get('CFLAGS', '')):
                args.append(cflag)

        # Make sure to list the library to be introspected first since it's
        # likely to be uninstalled yet and we want the uninstalled RPATHs have
        # priority (or we might run with installed library that is older)
        for source in sources:
            if not os.path.exists(source):
                raise CompilerError(
                    "Could not find object file: %s" % (source, ))

        args.extend(sources)

        pkg_config_libs = pkgconfig.libs(self._packages,
                                         msvc_syntax=self._compiler.check_is_msvc())

        if not self._options.external_library:
            self._compiler.get_internal_link_flags(args,
                                                   libtool,
                                                   self._options.libraries,
                                                   self._options.extra_libraries,
                                                   self._options.library_paths,
                                                   self._options.lib_dirs_envvar)
            args.extend(pkg_config_libs)

        else:
            args.extend(pkg_config_libs)
            self._compiler.get_external_link_flags(args, self._options.libraries)

        if not self._compiler.check_is_msvc():
            for ldflag in shlex.split(os.environ.get('LDFLAGS', '')):
                args.append(ldflag)

        dll_dirs = utils.dll_dirs()
        dll_dirs.add_dll_dirs(self._packages)

        if not self._options.quiet:
            print("g-ir-scanner: link: %s" % (
                subprocess.list2cmdline(args), ))
            sys.stdout.flush()

        msys = os.environ.get('MSYSTEM', None)
        if msys:
            shell = os.environ.get('SHELL', 'sh.exe')
            # Create a temporary script file that
            # runs the command we want
            tf, tf_name = tempfile.mkstemp()
            with os.fdopen(tf, 'wb') as f:
                shellcontents = ' '.join([x.replace('\\', '/') for x in args])
                fcontents = '#!/bin/sh\nunset PWD\n{}\n'.format(shellcontents)
                if not isinstance(fcontents, bytes):
                    fcontents = fcontents.encode('utf-8')
                f.write(fcontents)
            shell = utils.which(shell)
            args = [shell, tf_name.replace('\\', '/')]
        try:
            subprocess.check_call(args)
        except subprocess.CalledProcessError as e:
            raise LinkerError(e)
        finally:
            if msys:
                os.remove(tf_name)
            dll_dirs.cleanup_dll_dirs()


def compile_introspection_binary(options, get_type_functions,
                                 error_quark_functions):
    dc = DumpCompiler(options, get_type_functions, error_quark_functions)
    return dc.run()
