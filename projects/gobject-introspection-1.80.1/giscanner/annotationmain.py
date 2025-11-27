# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2010 Johan Dahlin
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

import sys
import optparse
import codecs
from contextlib import contextmanager

import giscanner
from giscanner.annotationparser import GtkDocCommentBlockParser, GtkDocCommentBlockWriter
from giscanner.scannermain import (get_preprocessor_option_group,
                                   create_source_scanner,
                                   process_packages)


@contextmanager
def encode_stdout(encoding):
    """Force stdout into a specific encoding."""
    # Python 3 uses a io.TextIOBase wrapped stdout with the system default encoding.
    # Re-wrap the underlying buffer with a new writer with the given 'encoding'.
    # See: https://docs.python.org/3/library/sys.html#sys.stdout
    old_stdout = sys.stdout
    binary_stdout = sys.stdout.buffer

    sys.stdout = codecs.getwriter(encoding)(binary_stdout)
    yield
    sys.stdout = old_stdout


def annotation_main(args):
    parser = optparse.OptionParser('%prog [options] sources',
                                   version='%prog ' + giscanner.__version__)

    group = optparse.OptionGroup(parser, "Tool modes, one is required")
    group.add_option("-e", "--extract",
                     action="store_true", dest="extract",
                     help="Extract annotations from the input files")
    parser.add_option_group(group)

    group = get_preprocessor_option_group(parser)
    group.add_option("-L", "--library-path",
                     action="append", dest="library_paths", default=[],
                     help="directories to search for libraries")
    group.add_option("", "--pkg",
                     action="append", dest="packages", default=[],
                     help="pkg-config packages to get cflags from")
    parser.add_option_group(group)

    options, args = parser.parse_args(args)

    if not options.extract:
        raise SystemExit("ERROR: Nothing to do")

    if options.packages:
        process_packages(options, options.packages)

    ss, _ = create_source_scanner(options, args)

    if options.extract:
        parser = GtkDocCommentBlockParser()
        writer = GtkDocCommentBlockWriter(indent=False)
        blocks = parser.parse_comment_blocks(ss.get_comments())

        with encode_stdout('utf-8'):
            print('/' + ('*' * 60) + '/')
            print('/* THIS FILE IS GENERATED DO NOT EDIT */')
            print('/' + ('*' * 60) + '/')
            print('')
            for block in sorted(blocks.values()):
                print(writer.write(block))
                print('')
            print('')
            print('/' + ('*' * 60) + '/')
            print('/* THIS FILE IS GENERATED DO NOT EDIT */')
            print('/' + ('*' * 60) + '/')

    return 0
