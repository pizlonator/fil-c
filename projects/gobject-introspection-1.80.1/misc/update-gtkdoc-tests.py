#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -*- Mode: Python -*-

# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2012-2015 Dieter Verfaillie <dieterv@optionexplicit.be>
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
import io

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from giscanner.annotationparser import (COMMENT_BLOCK_START_RE, COMMENT_BLOCK_END_RE,
                                        GtkDocCommentBlockParser, GtkDocCommentBlockWriter)
from giscanner.message import MessageLogger, ERROR, FATAL


HEADER = '''<?xml version="1.0" encoding="UTF-8"?>

<tests xmlns="http://schemas.gnome.org/gobject-introspection/2013/test">

'''

TEST = '''<test>
  <input>%(docblock)s</input>
  <parser>
%(tree)s
  </parser>
  <output>%(serialized)s</output>
</test>

'''

FOOTER = '''</tests>
'''


def parsed2tree(docblock):
    parsed = ''

    if docblock is not None:
        parsed += '    <docblock>\n'

        parsed += '      <identifier>\n'
        parsed += '        <name>%s</name>\n' % (docblock.name, )
        if docblock.annotations:
            parsed += '        <annotations>\n'
            for ann_name, ann_options in docblock.annotations.items():
                parsed += '          <annotation>\n'
                parsed += '            <name>%s</name>\n' % (ann_name, )
                if ann_options:
                    parsed += '            <options>\n'
                    if isinstance(ann_options, list):
                        for option in ann_options:
                            parsed += '              <option>\n'
                            parsed += '                <name>%s</name>\n' % (option, )
                            parsed += '              </option>\n'
                    else:
                        for (option, value) in ann_options.items():
                            parsed += '              <option>\n'
                            parsed += '                <name>%s</name>\n' % (option, )
                            if value:
                                parsed += '                <value>%s</value>\n' % (value, )
                            parsed += '              </option>\n'
                    parsed += '            </options>\n'
                parsed += '          </annotation>\n'
            parsed += '        </annotations>\n'
        parsed += '      </identifier>\n'

        if docblock.params:
            parsed += '      <parameters>\n'
            for param_name in docblock.params:
                param = docblock.params.get(param_name)
                parsed += '        <parameter>\n'
                parsed += '          <name>%s</name>\n' % (param_name, )
                if param.annotations:
                    parsed += '          <annotations>\n'
                    for ann_name, ann_options in param.annotations.items():
                        parsed += '            <annotation>\n'
                        parsed += '              <name>%s</name>\n' % (ann_name, )
                        if ann_options:
                            parsed += '              <options>\n'
                            if isinstance(ann_options, list):
                                for option in ann_options:
                                    parsed += '                <option>\n'
                                    parsed += '                  <name>%s</name>\n' % (option, )
                                    parsed += '                </option>\n'
                            else:
                                for (option, value) in ann_options.items():
                                    parsed += '                <option>\n'
                                    parsed += '                  <name>%s</name>\n' % (option, )
                                    if value:
                                        parsed += '                  <value>%s</value>\n' % (value, )
                                    parsed += '                </option>\n'
                            parsed += '              </options>\n'
                        parsed += '            </annotation>\n'
                    parsed += '          </annotations>\n'
                if param.description:
                    parsed += '          <description>%s</description>\n' % (param.description, )
                parsed += '        </parameter>\n'
            parsed += '      </parameters>\n'

        if docblock.description:
            parsed += '      <description>%s</description>\n' % (docblock.description, )

        if docblock.tags:
            parsed += '      <tags>\n'
            for tag_name in docblock.tags:
                tag = docblock.tags.get(tag_name)
                parsed += '        <tag>\n'
                parsed += '          <name>%s</name>\n' % (tag_name, )
                if tag.annotations:
                    parsed += '          <annotations>\n'
                    for ann_name, ann_options in tag.annotations.items():
                        parsed += '            <annotation>\n'
                        parsed += '              <name>%s</name>\n' % (ann_name, )
                        if ann_options:
                            parsed += '              <options>\n'
                            if isinstance(ann_options, list):
                                for option in ann_options:
                                    parsed += '                <option>\n'
                                    parsed += '                  <name>%s</name>\n' % (option, )
                                    parsed += '                </option>\n'
                            else:
                                for (option, value) in ann_options.items():
                                    parsed += '                <option>\n'
                                    parsed += '                  <name>%s</name>\n' % (option, )
                                    if value:
                                        parsed += '                  <value>%s</value>\n' % (value, )
                                    parsed += '                </option>\n'
                            parsed += '              </options>\n'
                        parsed += '            </annotation>\n'
                    parsed += '          </annotations>\n'
                if tag.value:
                    parsed += '          <value>%s</value>\n' % (tag.value, )
                if tag.description:
                    parsed += '          <description>%s</description>\n' % (tag.description, )
                parsed += '        </tag>\n'
            parsed += '      </tags>\n'

        parsed += '    </docblock>'

    return parsed


if __name__ == '__main__':
    gi_tests = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'tests', 'scanner', 'annotationparser', 'gtkdoc'))

    gtkdoc_tests = None
    if len(sys.argv) == 2:
        gtkdoc_tests = sys.argv[1]
    else:
        # Fall back to default jhbuild checkoutroot directory layout
        gtkdoc_tests = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'gtk-doc', 'tests'))

    if not gtkdoc_tests or not os.path.isdir(gtkdoc_tests):
        print('GTK-Doc source directory "%s" not found' % gtkdoc_tests)
        sys.exit(1)
    else:
        print('Using GTK-Doc source directory "%s"' % gtkdoc_tests)

    for root, dirs, files in os.walk(gtkdoc_tests):
        for inputfile in files:
            ext = os.path.splitext(inputfile)[-1].lower()
            if 'src' in root and ext in ['.c', '.h']:
                path = os.path.join(root, inputfile)
                print('Reading "%s"' % path)

                relpath = os.path.relpath(path, gtkdoc_tests).split(os.sep)
                relpath = os.path.join(relpath[0], relpath[len(relpath) - 1])

                logger = MessageLogger.get(namespace=None)
                parser = GtkDocCommentBlockParser()
                writer = GtkDocCommentBlockWriter(indent=True)
                logger.enable_warnings(True)

                with io.open(path, 'r', encoding='utf-8') as f:
                    lines = f.readlines()

                chunks = []
                in_comment = False
                chunk_start = 0
                chunk_end = 0

                for line_index, line in enumerate(lines):
                    if not in_comment:
                        if COMMENT_BLOCK_START_RE.match(line):
                            # We are at a line that starts a GTK-Doc comment block
                            in_comment = True
                            # Store where this GTK-Doc comment block starts
                            chunk_start = line_index
                    else:
                        if COMMENT_BLOCK_END_RE.match(line):
                            # We are at a line that closes a GTK-Doc comment block
                            in_comment = False
                            # Store the GTK-Doc comment block
                            chunks.append([''.join(lines[chunk_start:line_index + 1]), relpath, chunk_start + 1])
                            # Data chunk after the GTK-Doc comment block starts at the next line
                            chunk_start = line_index + 1
                            # Store where the GTK-Doc comment block ends
                            chunk_end = line_index

                outputfile = os.path.join(gi_tests, '%s.xml.in' % relpath)
                print('Writing "%s"' % outputfile)
                with io.open(outputfile, 'w', newline='', encoding='utf-8') as f:
                    f.write(HEADER)
                    for chunk in chunks:
                        docblock = parser.parse_comment_block(chunk[0].rstrip(), chunk[1], chunk[2])
                        if docblock:
                            f.write(TEST % {'docblock': chunk[0].rstrip(),
                                            'tree': parsed2tree(docblock),
                                            'serialized': writer.write(docblock).rstrip()})
                    f.write(FOOTER)
