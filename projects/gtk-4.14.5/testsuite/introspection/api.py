#! /usr/bin/env python3

import os
import sys

# Python 3.8.x or later on Windows require os.add_dll_directory()
# to be called on every directory that contains the required
# non-bundled, non-system DLLs of a module so that the module can
# be loaded successfully by Python.  Make things easiler for people
# by calling os.add_dll_directory() on the valid paths in %PATH%.
if hasattr(os, 'add_dll_directory'):
    paths = reversed(os.environ['PATH'].split(os.pathsep))
    for path in paths:
        if path != '' and os.path.isdir(path):
            os.add_dll_directory(path)

try:
    import gi
except ImportError:
    sys.exit(77) # skip this test, gi module is not available

gi.require_version('Gtk', '4.0')

from gi.repository import Gtk

assert isinstance(Gtk.INVALID_LIST_POSITION, int), 'Gtk.INVALID_LIST_POSITION is not an int'
