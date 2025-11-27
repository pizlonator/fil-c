# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008  Johan Dahlin
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

import re
import os
import subprocess
import platform
import shutil
import sys
import time
import giscanner.pkgconfig
from typing import Dict


_debugflags = None


def have_debug_flag(flag):
    """Check for whether a specific debugging feature is enabled.
Well-known flags:
 * start: Drop into debugger just after processing arguments
 * exception: Drop into debugger on fatalexception
 * warning: Drop into debugger on warning
 * posttrans: Drop into debugger just before introspectable pass
"""
    global _debugflags
    if _debugflags is None:
        _debugflags = os.environ.get('GI_SCANNER_DEBUG', '').split(',')
        if '' in _debugflags:
            _debugflags.remove('')
    return flag in _debugflags


def break_on_debug_flag(flag):
    if have_debug_flag(flag):
        import pdb
        pdb.set_trace()


# Copied from h2defs.py
_upperstr_pat1 = re.compile(r'([^A-Z])([A-Z])')
_upperstr_pat2 = re.compile(r'([A-Z][A-Z])([A-Z][0-9a-z])')
_upperstr_pat3 = re.compile(r'^([A-Z])([A-Z])')


def to_underscores(name):
    """Converts a typename to the equivalent underscores name.
    This is used to form the type conversion macros and enum/flag
    name variables.
    In particular, and differently from to_underscores_noprefix(),
    this function treats the first character differently if it is
    uppercase and followed by another uppercase letter."""
    name = _upperstr_pat1.sub(r'\1_\2', name)
    name = _upperstr_pat2.sub(r'\1_\2', name)
    name = _upperstr_pat3.sub(r'\1_\2', name, count=1)
    return name


def to_underscores_noprefix(name):
    """Like to_underscores, but designed for "unprefixed" names.
    to_underscores("DBusFoo") => dbus_foo, not d_bus_foo."""
    name = _upperstr_pat1.sub(r'\1_\2', name)
    name = _upperstr_pat2.sub(r'\1_\2', name)
    return name


_libtool_pat = re.compile("dlname='([A-z0-9\\.\\-\\+]+)'\n")


def _extract_dlname_field(la_file):
    with open(la_file, encoding='utf-8') as f:
        data = f.read()
    m = _libtool_pat.search(data)
    if m:
        return m.groups()[0]
    else:
        return None


_libtool_libdir_pat = re.compile("libdir='([^']+)'")


def _extract_libdir_field(la_file):
    with open(la_file, encoding='utf-8') as f:
        data = f.read()
    m = _libtool_libdir_pat.search(data)
    if m:
        return m.groups()[0]
    else:
        return None


# Returns the name that we would pass to dlopen() the library
# corresponding to this .la file
def extract_libtool_shlib(la_file):
    dlname = _extract_dlname_field(la_file)
    if dlname is None:
        return None

    # Darwin uses absolute paths where possible; since the libtool files never
    # contain absolute paths, use the libdir field
    if platform.system() == 'Darwin':
        dlbasename = os.path.basename(dlname)
        libdir = _extract_libdir_field(la_file)
        if libdir is None:
            return dlbasename
        return libdir + '/' + dlbasename
    # Older libtools had a path rather than the raw dlname
    return os.path.basename(dlname)


# Returns arguments for invoking libtool, if applicable, otherwise None
def get_libtool_command(options):
    libtool_infection = not options.nolibtool
    if not libtool_infection:
        return None

    libtool_path = options.libtool_path
    if libtool_path:
        # Automake by default sets:
        # LIBTOOL = $(SHELL) $(top_builddir)/libtool
        # To be strictly correct we would have to parse shell.  For now
        # we simply split().
        return libtool_path.split(' ')

    libtool_cmd = 'libtool'
    if platform.system() == 'Darwin':
        # libtool on OS X is a completely different program written by Apple
        libtool_cmd = 'glibtool'
    try:
        subprocess.check_call([libtool_cmd, '--version'],
                              stdout=open(os.devnull, 'w'))
    except (subprocess.CalledProcessError, OSError):
        # If libtool's not installed, assume we don't need it
        return None

    return [libtool_cmd]


def files_are_identical(path1, path2):
    with open(path1, 'rb') as f1, open(path2, 'rb') as f2:
        buf1 = f1.read(8192)
        buf2 = f2.read(8192)
        while buf1 == buf2 and buf1 != b'':
            buf1 = f1.read(8192)
            buf2 = f2.read(8192)
        return buf1 == buf2


def cflag_real_include_path(cflag):
    if not cflag.startswith("-I"):
        return cflag

    return "-I" + os.path.realpath(cflag[2:])


def host_os():
    return os.environ.get("GI_HOST_OS", os.name)


def which(program):
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    def is_nt_exe(fpath):
        return not fpath.lower().endswith('.exe') and \
            os.path.isfile(fpath + '.exe') and \
            os.access(fpath + '.exe', os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
        if os.name == 'nt' and is_nt_exe(program):
            return program + '.exe'
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file
            if os.name == 'nt' and is_nt_exe(exe_file):
                return exe_file + '.exe'

    return None


def get_user_cache_dir(dir=None):
    '''
    This is a Python reimplemention of `g_get_user_cache_dir()` because we don't want to
    rely on the python-xdg package and we can't depend on GLib via introspection.
    If any changes are made to that function they'll need to be copied here.
    '''

    xdg_cache_home = os.environ.get('XDG_CACHE_HOME')
    if xdg_cache_home is not None:
        if dir is not None:
            xdg_cache_home = os.path.join(xdg_cache_home, dir)
        try:
            os.makedirs(xdg_cache_home, mode=0o755, exist_ok=True)
        except EnvironmentError:
            # Let's fall back to ~/.cache below
            pass
        else:
            return xdg_cache_home

    homedir = os.path.expanduser('~')
    if homedir is not None:
        cachedir = os.path.join(homedir, '.cache')
        if dir is not None:
            cachedir = os.path.join(cachedir, dir)
        try:
            os.makedirs(cachedir, mode=0o755, exist_ok=True)
        except EnvironmentError:
            return None
        else:
            return cachedir

    return None


def get_user_data_dir():
    '''
    This is a Python reimplemention of `g_get_user_data_dir()` because we don't want to
    rely on the python-xdg package and we can't depend on GLib via introspection.
    If any changes are made to that function they'll need to be copied here.
    '''

    xdg_data_home = os.environ.get('XDG_DATA_HOME')
    if xdg_data_home is not None:
        try:
            os.makedirs(xdg_data_home, mode=0o700, exist_ok=True)
        except EnvironmentError:
            # Let's fall back to ~/.local/share below
            pass
        else:
            return xdg_data_home

    homedir = os.path.expanduser('~')
    if homedir is not None:
        datadir = os.path.join(homedir, '.local', 'share')
        try:
            os.makedirs(datadir, mode=0o700, exist_ok=True)
        except EnvironmentError:
            return None
        else:
            return datadir

    return None


def get_system_data_dirs():
    '''
    This is a Python reimplemention of `g_get_system_data_dirs()` because we don't want to
    rely on the python-xdg package and we can't depend on GLib via introspection.
    If any changes are made to that function they'll need to be copied here.
    '''
    xdg_data_dirs = [x for x in os.environ.get('XDG_DATA_DIRS', '').split(os.pathsep)]
    if not any(xdg_data_dirs) and os.name != 'nt':
        xdg_data_dirs.append('/usr/local/share')
        xdg_data_dirs.append('/usr/share')

    return xdg_data_dirs


def rmtree(*args, **kwargs):
    '''
    A variant of shutil.rmtree() which waits and tries again in case one of
    the files in the directory tree can't be deleted.

    This can be the case if a file is still in use by some process,
    for example a Virus scanner on Windows scanning .exe files when they are
    created.
    '''

    tries = 3
    for i in range(1, tries + 1):
        try:
            shutil.rmtree(*args, **kwargs)
        except OSError:
            if i == tries:
                raise
            time.sleep(i)
            continue
        else:
            return


class Singleton(type):
    '''
    A helper class to be used as metaclass to implement a singleton.
    '''
    _instances: Dict[type, object] = {}

    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            cls._instances[cls] = super(Singleton, cls).__call__(*args, **kwargs)
        return cls._instances[cls]


# Mainly used for builds against Python 3.8.x and later on Windows where we need to be
# more explicit on where dependent DLLs are located, via the use of
# os.add_dll_directory().
# To acquire the paths where dependent DLLs could be found we use:
#  *  The envvar GI_EXTRA_BASE_DLL_DIRS
#  *  The bindir variable from gio-2.0.pc when dependencies are installed in a prefix
#  *  -L directories from pkg-config --libs-only-L for uninstalled dependencies
class dll_dirs(metaclass=Singleton):
    _cached_dll_dirs = None
    _cached_added_dll_dirs = None

    def __init__(self):
        if os.name == 'nt' and hasattr(os, 'add_dll_directory'):
            self._cached_dll_dirs = []
            self._cached_added_dll_dirs = []

    def add_dll_dirs(self, pkgs):
        if os.name == 'nt' and hasattr(os, 'add_dll_directory'):
            if 'GI_EXTRA_BASE_DLL_DIRS' in os.environ:
                for path in os.environ.get('GI_EXTRA_BASE_DLL_DIRS').split(os.pathsep):
                    self._add_dll_dir(path)

            for path in giscanner.pkgconfig.libs_only_L(pkgs, True):
                libpath = path.replace('-L', '')
                self._add_dll_dir(libpath)

            for path in giscanner.pkgconfig.bindir(pkgs):
                self._add_dll_dir(path)

    def cleanup_dll_dirs(self):
        if self._cached_added_dll_dirs is not None:
            for added_dll_dir in self._cached_added_dll_dirs:
                added_dll_dir.close()
            self._cached_added_dll_dirs.clear()
        if self._cached_dll_dirs is not None:
            self._cached_dll_dirs.clear()

    def _add_dll_dir(self, path):
        if path not in self._cached_dll_dirs:
            self._cached_dll_dirs.append(path)
            self._cached_added_dll_dirs.append(os.add_dll_directory(path))


# monkey patch distutils.cygwinccompiler
# somehow distutils returns runtime library only up to
# VS2010 / MSVC 10.0 (msc_ver 1600)
def get_msvcr_overwrite():
    try:
        return orig_get_msvcr()
    except ValueError:
        pass

    msc_pos = sys.version.find('MSC v.')
    if msc_pos != -1:
        msc_ver = sys.version[msc_pos + 6:msc_pos + 10]

        if msc_ver == '1700':
            # VS2012
            return ['msvcr110']
        elif msc_ver == '1800':
            # VS2013
            return ['msvcr120']
        elif msc_ver >= '1900':
            # VS2015
            return ['vcruntime140']


import distutils.cygwinccompiler
orig_get_msvcr = distutils.cygwinccompiler.get_msvcr  # type: ignore
distutils.cygwinccompiler.get_msvcr = get_msvcr_overwrite  # type: ignore
