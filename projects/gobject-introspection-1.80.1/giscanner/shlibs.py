# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2009 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import os
import sys
import platform
import re
import subprocess

from .utils import get_libtool_command, extract_libtool_shlib, host_os
from .ccompiler import CCompiler


# For .la files, the situation is easy.
def _resolve_libtool(options, binary, libraries):
    shlibs = []
    for library in libraries:
        shlib = extract_libtool_shlib(library)
        if shlib:
            shlibs.append(shlib)

    return shlibs


# Assume ldd output is something vaguely like
#
#  libpangoft2-1.0.so.0 => /usr/lib/libpangoft2-1.0.so.0 (0x006c1000)
#
# We say that if something in the output looks like libpangoft2<blah>
# then the *first* such in the output is the soname.
def _ldd_library_pattern(library_name):
    return re.compile(r"""^
    # Require trailing slash to avoid matching liblibfoo when looking for libfoo.
    (.*[/])?
    lib%s
    # Prohibit library name characters to avoid matching libpangoft2 when looking for libpango.
    [^/A-Za-z0-9_-]
    # Anything but the path separator to avoid matching directories.
    [^/]*
    $""" % re.escape(library_name), re.VERBOSE)


# This is a what we do for non-la files. We assume that we are on an
# ELF-like system where ldd exists and the soname extracted with ldd is
# a filename that can be opened with dlopen().
#
# On OS X this will need a straightforward alternate implementation
# in terms of otool.
#
# Windows is more difficult, since there isn't always a straightforward
# translation between library name (.lib) and the name of the .dll, so
# extracting the dll names from the compiled app may not be sufficient.
# We might need to hunt down the .lib in the compile-time path and
# use that to figure out the name of the DLL.
#
def _resolve_non_libtool(options, binary, libraries):
    if not libraries:
        return []

    if platform.platform().startswith('OpenBSD'):
        # Hack for OpenBSD when using the ports' libtool which uses slightly
        # different directories to store the libraries in. So rewite binary.args[0]
        # by inserting '.libs/'.
        old_argdir = binary.args[0]
        new_libsdir = os.path.join(os.path.dirname(binary.args[0]), '.libs/')
        new_lib = new_libsdir + os.path.basename(binary.args[0])
        if os.path.exists(new_lib):
            binary.args[0] = new_lib
            os.putenv('LD_LIBRARY_PATH', new_libsdir)
        else:
            binary.args[0] = old_argdir

    if host_os() == 'nt':
        cc = CCompiler()
        return cc.resolve_windows_libs(libraries, options)
    else:
        args = []
        libtool = get_libtool_command(options)
        if libtool:
            args.extend(libtool)
            args.append('--mode=execute')
        platform_system = platform.system()
        if options.ldd_wrapper:
            args.extend([options.ldd_wrapper, binary.args[0]])
        elif platform_system == 'Darwin':
            args.extend(['otool', '-L', binary.args[0]])
        else:
            args.extend(['ldd', binary.args[0]])
        output = subprocess.check_output(args)
        if isinstance(output, bytes):
            output = output.decode("utf-8", "replace")

        shlibs = resolve_from_ldd_output(libraries, output)
        return list(map(sanitize_shlib_path, shlibs))


def sanitize_shlib_path(lib):
    # Use absolute paths on OS X to conform to how libraries are usually
    # referenced on OS X systems, and file names everywhere else.
    # In case we get relative paths on macOS (like @rpath) then we fall
    # back to the basename as well:
    # https://gitlab.gnome.org/GNOME/gobject-introspection/issues/222
    if sys.platform == "darwin":
        if not os.path.isabs(lib):
            return os.path.basename(lib)
        return lib
    else:
        return os.path.basename(lib)


def resolve_from_ldd_output(libraries, output):
    patterns = {}
    for library in libraries:
        if not os.path.isfile(library):
            patterns[library] = _ldd_library_pattern(library)
    if len(patterns) == 0:
        return []

    shlibs = []
    for line in output.splitlines():
        # ldd on *BSD show the argument passed on the first line even if
        # there is only one argument. We have to ignore it because it is
        # possible for the name of the binary to match _ldd_library_pattern.
        if line.endswith(':'):
            continue
        for word in line.split():
            for library, pattern in patterns.items():
                m = pattern.match(word)
                if m:
                    del patterns[library]
                    shlibs.append(m.group())
                    break

    if len(patterns) > 0:
        raise SystemExit(
            "ERROR: can't resolve libraries to shared libraries: " +
            ", ".join(patterns.keys()))

    return shlibs


# We want to resolve a set of library names (the <foo> of -l<foo>)
# against a library to find the shared library name. The shared
# library name is suppose to be what you pass to dlopen() (or
# equivalent). And we want to do this using the libraries that 'binary'
# is linking against.
#
def resolve_shlibs(options, binary, libraries):
    libtool = list(filter(lambda x: x.endswith(".la"), libraries))
    non_libtool = list(filter(lambda x: not x.endswith(".la"), libraries))

    return (_resolve_libtool(options, binary, libtool) +
            _resolve_non_libtool(options, binary, non_libtool))
