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
import shlex
import subprocess
import tempfile

import sys
import distutils

from distutils.msvccompiler import MSVCCompiler
from distutils.unixccompiler import UnixCCompiler
from distutils.cygwinccompiler import Mingw32CCompiler
from distutils.sysconfig import get_config_vars
from distutils.sysconfig import customize_compiler as orig_customize_compiler

from . import utils


def customize_compiler(compiler):
    """This is a version of distutils.sysconfig.customize_compiler, without
    any macOS specific bits and which tries to avoid using any Python specific
    defaults if alternatives through env vars are given.
    """

    # The original customize_compiler() in distutils calls into macOS setup
    # code the first time it is called. This makes sure we run that setup
    # code as well.
    dummy = distutils.ccompiler.new_compiler()
    orig_customize_compiler(dummy)

    if compiler.compiler_type == "unix":
        (cc, cxx, ldshared, shlib_suffix, ar, ar_flags) = \
            get_config_vars('CC', 'CXX', 'LDSHARED', 'SHLIB_SUFFIX', 'AR', 'ARFLAGS')

        if 'CC' in os.environ:
            newcc = os.environ['CC']
            if 'LDSHARED' not in os.environ and ldshared.startswith(cc):
                ldshared = newcc + ldshared[len(cc):]
            cc = newcc
        if 'CXX' in os.environ:
            cxx = os.environ['CXX']
        if 'LDSHARED' in os.environ:
            ldshared = os.environ['LDSHARED']
        if 'CPP' in os.environ:
            cpp = os.environ['CPP']
        else:
            cpp = cc + " -E"
        if 'LDFLAGS' in os.environ:
            ldshared = ldshared + ' ' + os.environ['LDFLAGS']
        if 'CFLAGS' in os.environ:
            cflags = os.environ['CFLAGS']
            ldshared = ldshared + ' ' + os.environ['CFLAGS']
        else:
            cflags = ''
        if 'CPPFLAGS' in os.environ:
            cpp = cpp + ' ' + os.environ['CPPFLAGS']
            cflags = cflags + ' ' + os.environ['CPPFLAGS']
            ldshared = ldshared + ' ' + os.environ['CPPFLAGS']
        if 'AR' in os.environ:
            ar = os.environ['AR']
        if 'ARFLAGS' in os.environ:
            archiver = ar + ' ' + os.environ['ARFLAGS']
        else:
            archiver = ar + ' ' + ar_flags

        cc_cmd = cc + ' ' + cflags
        compiler.set_executables(
            preprocessor=cpp,
            compiler=cc_cmd,
            compiler_so=cc_cmd,
            compiler_cxx=cxx,
            linker_so=ldshared,
            linker_exe=cc,
            archiver=archiver)

        compiler.shared_lib_extension = shlib_suffix


def resolve_mingw_lib(implib, libtool=None):
    """Returns a DLL name given a path to an import lib

    /full/path/to/libgtk-3.dll.a -> libgtk-3-0.dll
    """

    args = []
    if libtool:
        args.extend(libtool)
        args.append('--mode=execute')

    # Figure out if we have a gcc toolchain or llvm one
    dlltool = os.environ.get('DLLTOOL', 'dlltool.exe')
    dlltool_output = subprocess.run(
        [dlltool], stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True).stdout
    is_llvm = 'llvm-dlltool' in dlltool_output

    if not is_llvm:
        # gcc dlltool provides this via --identify
        dlltool_args = args + [dlltool, '--identify']
        output = subprocess.check_output(dlltool_args + [implib], universal_newlines=True)
        for line in output.splitlines():
            return line
    else:
        # for llvm we need to parse the output of nm
        # https://github.com/msys2/MINGW-packages/issues/11994#issuecomment-1176691216
        output = subprocess.check_output(args + ['nm', implib], universal_newlines=True)
        for line in output.splitlines():
            if line.endswith(':'):
                return line[:-1]
    return None


# Flags that retain macros in preprocessed output.
FLAGS_RETAINING_MACROS = ['-g3', '-ggdb3', '-gstabs3', '-gcoff3', '-gxcoff3', '-gvms3']


class CCompiler(object):

    compiler_cmd = ''
    compiler = None
    _cflags_no_deprecation_warnings = ''

    def __init__(self,
                 environ=os.environ,
                 osname=os.name,
                 compiler_name=None):

        if osname == 'nt':
            # The compiler used here on Windows may well not be
            # the same compiler that was used to build Python,
            # as the official Python binaries are built with
            # Visual Studio
            if compiler_name is None:
                if environ.get('MSYSTEM') == 'MINGW32' or environ.get('MSYSTEM') == 'MINGW64':
                    compiler_name = 'mingw32'
                else:
                    compiler_name = distutils.ccompiler.get_default_compiler()
            if compiler_name != 'msvc' and \
               compiler_name != 'mingw32':
                raise SystemExit('Specified Compiler \'%s\' is unsupported.' % compiler_name)
        else:
            if compiler_name is None:
                # XXX: Is it common practice to use a non-Unix compiler
                #      class instance on non-Windows on platforms g-i supports?
                compiler_name = distutils.ccompiler.get_default_compiler()

        # Now, create the distutils ccompiler instance based on the info we have.
        if compiler_name == 'msvc':
            # For MSVC, we need to create a instance of a subclass of distutil's
            # MSVC9Compiler class, as it does not provide a preprocess()
            # implementation
            from . import msvccompiler
            self.compiler = msvccompiler.get_msvc_compiler()
        else:
            self.compiler = distutils.ccompiler.new_compiler(compiler=compiler_name)
        customize_compiler(self.compiler)

        # customize_compiler() from distutils only does customization
        # for 'unix' compiler type.  Also, avoid linking to msvcrxx.dll
        # for MinGW builds as the dumper binary does not link to the
        # Python DLL, but link to msvcrt.dll if necessary.
        if isinstance(self.compiler, Mingw32CCompiler):
            if self.compiler.dll_libraries != ['msvcrt']:
                self.compiler.dll_libraries = []
            if self.compiler.preprocessor is None:
                self.compiler.preprocessor = self.compiler.compiler + ['-E']

        if self.check_is_msvc():
            # We trick distutils to believe that we are (always) using a
            # compiler supplied by a Windows SDK, so that we avoid launching
            # a new build environment to detect the compiler that is used to
            # build Python itself, which is not desirable, so that we use the
            # compiler commands (and env) as-is.
            os.environ['DISTUTILS_USE_SDK'] = '1'
            if 'MSSdk' not in os.environ:
                if 'WindowsSDKDir' in os.environ:
                    os.environ['MSSdk'] = os.environ.get('WindowsSDKDir')
                elif os.environ.get('VCInstallDir'):
                    os.environ['MSSdk'] = os.environ.get('VCInstallDir')

            if self.compiler.check_is_clang_cl():
                self.compiler_cmd = os.environ.get('CC').split()[0]
            else:
                self.compiler_cmd = 'cl.exe'
                self._cflags_no_deprecation_warnings = "-wd4996"
        else:
            if (isinstance(self.compiler, Mingw32CCompiler)):
                self.compiler_cmd = self.compiler.compiler[0]
            else:
                self.compiler_cmd = ' '.join(self.compiler.compiler)

            self._cflags_no_deprecation_warnings = "-Wno-deprecated-declarations"

    def get_internal_link_flags(self, args, libtool, libraries, extra_libraries, libpaths, lib_dirs_envvar):
        # An "internal" link is where the library to be introspected
        # is being built in the current directory.

        runtime_path_envvar = []
        runtime_paths = []

        if os.name == 'nt':
            runtime_path_envvar = ['LIB', 'PATH']
        else:
            runtime_path_envvar = ['LD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH'] if not lib_dirs_envvar else [lib_dirs_envvar]
            # Search the current directory first
            # (This flag is not supported nor needed for Visual C++)
            args.append('-L.')

            if not libtool:
                # https://bugzilla.gnome.org/show_bug.cgi?id=625195
                args.append('-Wl,-rpath,.')

                # Ensure libraries are always linked as we are going to use ldd to work
                # out their names later
                if sys.platform != 'darwin':
                    args.append('-Wl,--no-as-needed')

        for library_path in libpaths:
            # The dumper program needs to look for dynamic libraries
            # in the library paths first
            if self.check_is_msvc():
                library_path = library_path.replace('/', '\\')
                args.append('-libpath:' + library_path)
            else:
                args.append('-L' + library_path)
                if os.name != 'nt' and os.path.isabs(library_path):
                    if libtool:
                        args.append('-rpath')
                        args.append(library_path)
                    else:
                        args.append('-Wl,-rpath,' + library_path)

            runtime_paths.append(library_path)

        for library in libraries + extra_libraries:
            if os.path.isfile(library):
                # If we get a real filename, just use it as-is
                args.append(library)
            elif self.check_is_msvc():
                # Note that Visual Studio builds do not use libtool!
                if library != 'm':
                    args.append(library + '.lib')
            else:
                # If we get a real filename, just use it as-is
                if library.endswith(".la"):
                    args.append(library)
                else:
                    args.append('-l' + library)

        for envvar in runtime_path_envvar:
            if envvar in os.environ:
                os.environ[envvar] = \
                    os.pathsep.join(runtime_paths + [os.environ[envvar]])
            else:
                os.environ[envvar] = os.pathsep.join(runtime_paths)

    def get_external_link_flags(self, args, libraries):
        # An "external" link is where the library to be introspected
        # is installed on the system; this case is used for the scanning
        # of GLib in gobject-introspection itself.

        for library in libraries:
            if os.path.isfile(library):
                # If we get a real filename, just use it as-is
                args.append(library)
            elif self.check_is_msvc():
                # Note that Visual Studio builds do not use libtool!
                if library != 'm':
                    args.append(library + '.lib')
            else:
                if library.endswith(".la"):  # explicitly specified libtool library
                    args.append(library)
                else:
                    args.append('-l' + library)

    def preprocess(self, source, output, cpp_options):
        extra_postargs = ['-C']
        (include_paths, macros, postargs) = self._set_cpp_options(cpp_options)

        # We always want to include the current path
        include_dirs = ['.']

        include_dirs.extend(include_paths)
        extra_postargs.extend(postargs)

        # Define these macros when using Visual C++ to silence many warnings,
        # and prevent stepping on many Visual Studio-specific items, so that
        # we don't have to handle them specifically in scannerlexer.l
        if self.check_is_msvc() and not self.compiler.check_is_clang_cl():
            macros.append(('_USE_DECLSPECS_FOR_SAL', None))
            macros.append(('_CRT_SECURE_NO_WARNINGS', None))
            macros.append(('_CRT_NONSTDC_NO_WARNINGS', None))
            macros.append(('SAL_NO_ATTRIBUTE_DECLARATIONS', None))

        self.compiler.preprocess(source=source,
                                 output_file=output,
                                 macros=macros,
                                 include_dirs=include_dirs,
                                 extra_postargs=extra_postargs)

    def compile(self, pkg_config_cflags, cpp_includes, source, init_sections):
        extra_postargs = []
        includes = []
        (include_paths, macros, extra_args) = \
            self._set_cpp_options(pkg_config_cflags)

        for include in cpp_includes:
            includes.append(include)

        if isinstance(self.compiler, UnixCCompiler):
            # This is to handle the case where macros are defined in CFLAGS
            cflags = os.environ.get('CFLAGS')
            if cflags:
                for i, cflag in enumerate(shlex.split(cflags)):
                    if cflag.startswith('-D'):
                        stridx = cflag.find('=')
                        if stridx > -1:
                            macroset = (cflag[2:stridx],
                                        cflag[stridx + 1:])
                        else:
                            macroset = (cflag[2:], None)
                        if macroset not in macros:
                            macros.append(macroset)

        # Do not add -Wall when using init code as we do not include any
        # header of the library being introspected
        if self.compiler_cmd == 'gcc' and not init_sections:
            extra_postargs.append('-Wall')
        extra_postargs.append(self._cflags_no_deprecation_warnings)

        includes.extend(include_paths)
        extra_postargs.extend(extra_args)

        return self.compiler.compile(sources=source,
                                     macros=macros,
                                     include_dirs=includes,
                                     extra_postargs=extra_postargs,
                                     output_dir=os.path.abspath(os.sep))

    def resolve_windows_libs(self, libraries, options):
        args = []
        libsearch = []

        # When we are using Visual C++ or clang-cl...
        if self.check_is_msvc():
            # The search path of the .lib's on Visual C++
            # is dependent on the LIB environmental variable,
            # so just query for that
            libpath = os.environ.get('LIB')
            libsearch = libpath.split(';')

            # Use the dumpbin utility that's included in
            # every Visual C++ installation to find out which
            # DLL the .lib gets linked to.
            # dumpbin -symbols something.lib gives the
            # filename of DLL without the '.dll' extension that something.lib
            # links to, in the line that contains
            # __IMPORT_DESCRIPTOR_<dll_filename_that_something.lib_links_to>
            args.append('dumpbin.exe')
            args.append('-symbols')

        # When we are not using Visual C++ nor clang-cl (i.e. we are using GCC)...
        else:
            libtool = utils.get_libtool_command(options)
            proc = subprocess.Popen([self.compiler_cmd, '-print-search-dirs'],
                                    stdout=subprocess.PIPE)
            o, e = proc.communicate()
            libsearch = options.library_paths
            for line in o.decode('ascii').splitlines():
                if line.startswith('libraries: '):
                    libsearch += line[len('libraries: '):].split(os.pathsep)

        shlibs = []
        not_resolved = []
        for lib in libraries:
            found = False
            candidates = [
                'lib%s.dll.a' % lib,
                'lib%s.dll.lib' % lib,  # rust cdylib
                'lib%s.a' % lib,
                '%s.dll.a' % lib,
                '%s.a' % lib,
                '%s.lib' % lib,
            ]
            for l in libsearch:
                if found:
                    break
                if l.startswith('='):
                    l = l[1:]
                for c in candidates:
                    if found:
                        break
                    implib = os.path.join(l, c)
                    if os.path.exists(implib):
                        if self.check_is_msvc():
                            tmp_fd, tmp_filename = \
                                tempfile.mkstemp(prefix='g-ir-win32-resolve-lib-')

                            # This is dumb, but it is life... Windows does not like one
                            # trying to write to a file when its FD is not closed first,
                            # when we use a flag in a program to do so.  So, close,
                            # write to temp file with dumpbin and *then* re-open the
                            # file for reading.
                            os.close(tmp_fd)
                            output_flag = ['-out:' + tmp_filename]
                            proc = subprocess.call(args + [implib] + output_flag,
                                                   stdout=subprocess.PIPE)
                            with open(tmp_filename, 'r', encoding='utf-8') as tmp_fileobj:
                                for line in tmp_fileobj.read().splitlines():

                                    if '__IMPORT_DESCRIPTOR_' in line:
                                        line_tokens = line.split()
                                        for item in line_tokens:
                                            if item.startswith('__IMPORT_DESCRIPTOR_'):
                                                shlibs.append(item[20:] + '.dll')
                                                found = True
                                                break
                            tmp_fileobj.close()
                            os.unlink(tmp_filename)
                        else:
                            shlib = resolve_mingw_lib(implib, libtool)
                            if shlib is not None:
                                shlibs.append(shlib)
                                found = True
            if not found:
                not_resolved.append(lib)
        if len(not_resolved) > 0:
            raise SystemExit(
                "ERROR: can't resolve libraries to shared libraries: " +
                ", ".join(not_resolved))
        return shlibs

    @property
    def linker_cmd(self):
        if self.check_is_msvc():
            if not self.compiler.initialized:
                self.compiler.initialize()
            return [self.compiler.linker]
        else:
            return self.compiler.linker_exe

    def check_is_msvc(self):
        return isinstance(self.compiler, MSVCCompiler)

    # Private APIs
    def _set_cpp_options(self, options):
        includes = []
        macros = []
        other_options = []

        for o in options:
            option = utils.cflag_real_include_path(o)
            if option.startswith('-I'):
                includes.append(option[len('-I'):])
            elif option.startswith('-D'):
                macro = option[len('-D'):]
                macro_index = macro.find('=')
                if macro_index == -1:
                    macro_name = macro
                    macro_value = None
                else:
                    macro_name = macro[:macro_index]
                    macro_value = macro[macro_index + 1:]

                    # Somehow the quotes used in defining
                    # macros for compiling using distutils
                    # get dropped for MSVC builds, so
                    # escape the escape character.
                    if isinstance(self.compiler, MSVCCompiler):
                        macro_value = macro_value.replace('\"', '\\\"')
                macros.append((macro_name, macro_value))
            elif option.startswith('-U'):
                macros.append((option[len('-U'):],))
            else:
                # We expect the preprocessor to remove macros. If debugging is turned
                # up high enough that won't happen, so don't add those flags. Bug #720504
                if option not in FLAGS_RETAINING_MACROS:
                    other_options.append(option)
        return (includes, macros, other_options)
