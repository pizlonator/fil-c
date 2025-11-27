# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2014  Chun-wei Fan
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
import distutils

from distutils.errors import DistutilsExecError, CompileError
from distutils.ccompiler import CCompiler, gen_preprocess_options
from distutils.dep_util import newer

# Distutil's MSVCCompiler does not provide a preprocess()
# Implementation, so do our own here.


def get_msvc_compiler():
    return MSVCCompiler()


class MSVCCompiler(distutils.msvccompiler.MSVCCompiler):

    def __init__(self, verbose=0, dry_run=0, force=0):
        super(distutils.msvccompiler.MSVCCompiler, self).__init__()
        CCompiler.__init__(self, verbose, dry_run, force)
        self.__paths = []
        self.__arch = None  # deprecated name
        if os.name == 'nt':
            if isinstance(self, distutils.msvc9compiler.MSVCCompiler):
                self.__version = distutils.msvc9compiler.VERSION
        self.initialized = False
        self.preprocess_options = None
        if self.check_is_clang_cl():
            cc_cmd = os.environ.get('CC').split()
            self.cc = cc_cmd[0]
            self.linker = 'lld-link'
            self.compile_options = []
            # Add any arguments added to clang-cl to self.compile_options
            # such as cross-compilation flags
            if len(cc_cmd) > 1:
                self.compile_options.extend(cc_cmd[1:])
            self.initialized = True

    def preprocess(self,
                   source,
                   output_file=None,
                   macros=None,
                   include_dirs=None,
                   extra_preargs=None,
                   extra_postargs=None):
        if self.initialized is False:
            self.initialize()

        (_, macros, include_dirs) = \
            self._fix_compile_args(None, macros, include_dirs)
        pp_opts = gen_preprocess_options(macros, include_dirs)
        preprocess_options = ['-E']
        source_basename = None

        if output_file is not None:
            preprocess_options.append('-P')
            source_basename = self._get_file_basename(source)
        cpp_args = self.cc.split()
        if extra_preargs is not None:
            cpp_args[:0] = extra_preargs
        if extra_postargs is not None:
            preprocess_options.extend(extra_postargs)
        cpp_args.extend(preprocess_options)
        cpp_args.extend(pp_opts)
        cpp_args.append(source)

        # We need to preprocess: either we're being forced to, or the
        # source file is newer than the target (or the target doesn't
        # exist).
        if self.force or output_file is None or newer(source, output_file):
            try:
                self.spawn(cpp_args)
            except DistutilsExecError as msg:
                print(msg)
                raise CompileError

        # The /P option for the MSVC preprocessor will output the results
        # of the preprocessor to a file, as <source_without_extension>.i,
        # so in order to output the specified filename, we need to rename
        # that file
        if output_file is not None:
            if output_file != source_basename + '.i':
                os.rename(source_basename + '.i', output_file)

    def _get_file_basename(self, filename):
        if filename is None:
            return None
        if filename.rfind('.') == -1:
            return filename[filename.rfind('\\') + 1:]
        else:
            return filename[filename.rfind('\\') + 1:filename.rfind('.')]

    def check_is_clang_cl(self):
        # To run g-ir-scanner under Windows using clang-cl, set both `CC` and
        # `CXX` to `clang-cl [<arch_args>]` and ensure that clang-cl.exe and
        # lld-link.exe are in the PATH in a Visual Studio command prompt.  Note
        # that the Windows SDK is still needed in this case.  This is in line
        # with what is done in Meson
        return (os.environ.get('CC') is not None and
                os.environ.get('CXX') is not None and
                os.environ.get('CC').split()[0] == 'clang-cl' and
                os.environ.get('CXX').split()[0] == 'clang-cl')
