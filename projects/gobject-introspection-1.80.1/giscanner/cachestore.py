# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008-2010  Johan Dahlin
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

import errno
import glob
import hashlib
import os
import shutil
import sys
import tempfile
import pickle

import giscanner

from . import utils


_CACHE_VERSION_FILENAME = '.cache-version'


def _get_versionhash():
    toplevel = os.path.dirname(giscanner.__file__)
    sources = glob.glob(os.path.join(toplevel, '*.py'))
    sources.append(sys.argv[0])
    # Using mtimes is a bit (5x) faster than hashing the file contents
    mtimes = (str(os.stat(source).st_mtime) for source in sources)
    # ASCII encoding is sufficient since we are only dealing with numbers.
    return hashlib.sha1(''.join(mtimes).encode('ascii')).hexdigest()


class CacheStore(object):

    def __init__(self):
        self._directory = self._get_cachedir()
        self._check_cache_version()

    def _get_cachedir(self):
        if 'GI_SCANNER_DISABLE_CACHE' in os.environ:
            return None
        else:
            cachedir = utils.get_user_cache_dir('g-ir-scanner')
            return cachedir

    def _check_cache_version(self):
        if self._directory is None:
            return

        current_hash = _get_versionhash()
        version = os.path.join(self._directory, _CACHE_VERSION_FILENAME)
        try:
            with open(version, 'r', encoding='utf-8') as version_file:
                cache_hash = version_file.read()
        except (IOError, OSError) as e:
            # File does not exist
            if e.errno == errno.ENOENT:
                cache_hash = 0
            else:
                raise

        if current_hash == cache_hash:
            return

        self._clean()

        tmp_fd, tmp_filename = tempfile.mkstemp(prefix='g-ir-scanner-cache-version-')
        try:
            with os.fdopen(tmp_fd, 'w', encoding='utf-8') as tmp_file:
                tmp_file.write(current_hash)

            # On Unix, this would just be os.rename() but Windows
            # doesn't allow that.
            shutil.move(tmp_filename, version)
        except (IOError, OSError) as e:
            # Permission denied
            if e.errno == errno.EACCES:
                return
            else:
                raise

    def _get_filename(self, filename):
        # If we couldn't create the directory we're probably
        # on a read only home directory where we just disable
        # the cache all together.
        if self._directory is None:
            return
        # Assume UTF-8 encoding for the filenames. This doesn't matter so much
        # as long as the results of this method always produce the same hash.
        hexdigest = hashlib.sha1(filename.encode('utf-8')).hexdigest()
        return os.path.join(self._directory, hexdigest)

    def _cache_is_valid(self, store_filename, filename):
        try:
            store_mtime = os.stat(store_filename).st_mtime
        except FileNotFoundError:
            return False

        return store_mtime >= os.stat(filename).st_mtime

    def _remove_filename(self, filename):
        try:
            os.unlink(filename)
        except (IOError, OSError) as e:
            # Ignore "permission denied", "file does not exist"
            if e.errno in (errno.EACCES, errno.ENOENT):
                return
            else:
                raise

    def _clean(self):
        for filename in os.listdir(self._directory):
            if filename == _CACHE_VERSION_FILENAME:
                continue
            self._remove_filename(os.path.join(self._directory, filename))

    def store(self, filename, data):
        store_filename = self._get_filename(filename)
        if store_filename is None:
            return

        if self._cache_is_valid(store_filename, filename):
            return None

        tmp_fd, tmp_filename = tempfile.mkstemp(prefix='g-ir-scanner-cache-')
        try:
            with os.fdopen(tmp_fd, 'wb') as tmp_file:
                pickle.dump(data, tmp_file)
        except (IOError, OSError) as e:
            # No space left on device
            if e.errno == errno.ENOSPC:
                self._remove_filename(tmp_filename)
                return
            else:
                raise

        try:
            shutil.move(tmp_filename, store_filename)
        except (IOError, OSError) as e:
            # Permission denied
            if e.errno == errno.EACCES:
                self._remove_filename(tmp_filename)
            else:
                raise

    def load(self, filename):
        store_filename = self._get_filename(filename)
        if store_filename is None:
            return
        try:
            fd = open(store_filename, 'rb')
        except (IOError, OSError) as e:
            if e.errno == errno.ENOENT:
                return None
            else:
                raise

        with fd:
            if not self._cache_is_valid(store_filename, filename):
                return None
            try:
                data = pickle.load(fd)
            except Exception:
                # Broken cache entry, remove it
                self._remove_filename(store_filename)
                data = None
            return data
