# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008-2011 Johan Dahlin
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
import argparse

import giscanner
from .docwriter import DocWriter
from .sectionparser import generate_sections_file, write_sections_file
from .transformer import Transformer

FORMATS = ('devdocs', 'mallard', 'sections')


def doc_main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', action='version',
                      version='%(prog)s ' + giscanner.__version__)
    parser.add_argument("girfile")
    parser.add_argument("-o", "--output",
                      action="store", dest="output",
                      help="Directory to write output to")
    parser.add_argument("-l", "--language",
                      action="store", dest="language",
                      default="c",
                      help="Output language")
    parser.add_argument("-f", "--format",
                        action="store", dest="format",
                        choices=FORMATS, default=FORMATS[1],
                        help="Output format")
    parser.add_argument("-I", "--add-include-path",
                      action="append", dest="include_paths", default=[],
                      help="include paths for other GIR files")
    parser.add_argument("-s", "--write-sections-file",
                        action="store_const", dest="format", const="sections",
                        help="Backwards-compatible equivalent to -f sections")
    parser.add_argument("--templates-dir",
                        action="store")

    args = parser.parse_args(args[1:])
    if not args.output:
        raise SystemExit("missing output parameter")
    if args.format not in FORMATS:
        raise SystemExit("Unknown output format %s (supported: %s)" %
            (args.format, ", ".join(FORMATS)))

    if 'UNINSTALLED_INTROSPECTION_SRCDIR' in os.environ:
        top_srcdir = os.environ['UNINSTALLED_INTROSPECTION_SRCDIR']
        top_builddir = os.environ['UNINSTALLED_INTROSPECTION_BUILDDIR']
        extra_include_dirs = [os.path.join(top_srcdir, 'gir'), top_builddir]
    else:
        extra_include_dirs = []
    extra_include_dirs.extend(args.include_paths)
    transformer = Transformer.parse_from_gir(args.girfile, extra_include_dirs)

    if args.format == 'sections':
        sections_file = generate_sections_file(transformer)

        with open(args.output, 'w', encoding='utf-8') as fp:
            write_sections_file(fp, sections_file)
    else:
        writer = DocWriter(transformer, args.language, args.format, args.templates_dir)
        writer.write(args.output)

    return 0
