# -*- Mode: Python -*-
# Copyright (C) 2013 Hat, Inc.
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
from . import ast
from .utils import to_underscores


class SectionsFile(object):
    def __init__(self, sections):
        self.sections = sections


class Section(object):
    def __init__(self):
        self.file = None
        self.title = None
        self.includes = None
        self.subsections = []


class Subsection(object):
    def __init__(self, name):
        self.name = name
        self.symbols = []


def parse_sections_file(lines):
    sections = []
    current_section = None
    current_subsection = None

    for line in lines:
        line = line.rstrip()

        if not line or line.isspace():
            continue

        if line == "<SECTION>":
            current_section = Section()
            sections.append(current_section)
            current_subsection = Subsection(None)
            current_section.subsections.append(current_subsection)
            continue

        if line == "</SECTION>":
            current_section = None
            continue

        match = re.match(r"<FILE>(?P<contents>.*)</FILE>", line)
        if match:
            current_section.file = match.groupdict['contents']
            continue

        match = re.match(r"<TITLE>(?P<contents>.*)</TITLE>", line)
        if match:
            current_section.title = match.groupdict['contents']
            continue

        match = re.match(r"<INCLUDE>(?P<contents>.*)</INCLUDE>", line)
        if match:
            current_section.includes = match.groupdict['contents']
            continue

        match = re.match(r"<SUBSECTION(?: (?P<name>.*))?>", line)
        if match:
            current_subsection = Subsection(match.groupdict.get('name', None))
            current_section.subsections.append(current_subsection)
            continue

        if line.startswith("<") and line.endswith(">"):
            # Other directive to gtk-doc, not a symbol.
            continue

        current_subsection.symbols.append(line)

    return SectionsFile(sections)


def write_sections_file(f, sections_file):
    for section in sections_file.sections:
        f.write("\n<SECTION>\n")
        if section.file is not None:
            f.write("<FILE>%s</FILE>\n" % (section.file, ))
        if section.title is not None:
            f.write("<TITLE>%s</TITLE>\n" % (section.title, ))
        if section.includes is not None:
            f.write("<INCLUDE>%s</INCLUDE>\n" % (section.includes, ))

        is_first_subsection = True
        for subsection in section.subsections:
            if subsection.name is not None:
                f.write("<SUBSECTION %s>\n" % (subsection.name, ))
            elif not is_first_subsection:
                f.write("\n<SUBSECTION>\n")

            is_first_subsection = False

            for symbol in subsection.symbols:
                f.write(symbol + "\n")


def generate_sections_file(transformer):
    ns = transformer.namespace

    sections = []

    def new_section(file_, title):
        section = Section()
        section.file = file_
        section.title = title
        section.subsections.append(Subsection(None))
        sections.append(section)
        return section

    def append_symbol(section, sym):
        section.subsections[0].symbols.append(sym)

    general_section = new_section("main", "Main")

    for node in ns.values():
        if isinstance(node, ast.Function):
            append_symbol(general_section, node.symbol)
        elif isinstance(node, (ast.Class, ast.Interface)):
            gtype_name = node.gtype_name
            file_name = to_underscores(gtype_name).replace('_', '-').lower()
            section = new_section(file_name, gtype_name)
            append_symbol(section, gtype_name)
            append_symbol(section, node.glib_type_struct.target_giname.replace('.', ''))

            for meth in node.methods:
                append_symbol(section, meth.symbol)
            for meth in node.static_methods:
                append_symbol(section, meth.symbol)
        elif isinstance(node, ast.DocSection):
            section = new_section(None, node.name)

    return SectionsFile(sections)
