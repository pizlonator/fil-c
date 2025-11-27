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

from io import StringIO
from contextlib import contextmanager
from xml.sax.saxutils import escape, quoteattr


def _calc_attrs_length(attributes, indent, self_indent):
    if indent == -1:
        return -1
    attr_length = 0
    for attr, value in attributes:
        # FIXME: actually, if we have attributes with None as value this
        # should be considered a bug and raise an error. We are just
        # ignoring them here while we fix GIRParser to create the right
        # ast with the correct attributes.
        if value is None:
            continue
        attr_length += 2 + len(attr) + len(quoteattr(value))
    return attr_length + indent + self_indent


def collect_attributes(tag_name, attributes, self_indent, self_indent_char, indent=-1):
    if not attributes:
        return ''
    if _calc_attrs_length(attributes, indent, self_indent) > 79:
        indent_len = self_indent + len(tag_name) + 1
    else:
        indent_len = 0
    first = True
    attr_value = ''
    for attr, value in attributes:
        # FIXME: actually, if we have attributes with None as value this
        # should be considered a bug and raise an error. We are just
        # ignoring them here while we fix GIRParser to create the right
        # ast with the correct attributes.
        if value is None:
            continue
        if indent_len and not first:
            attr_value += '\n%s' % (self_indent_char * indent_len)
        attr_value += ' %s=%s' % (attr, quoteattr(value))
        if first:
            first = False
    return attr_value


def build_xml_tag(tag_name, attributes=None, data=None, self_indent=0,
                  self_indent_char=' '):
    if attributes is None:
        attributes = []
    prefix = '<%s' % (tag_name, )
    if data is not None:
        if isinstance(data, bytes):
            data = data.decode('UTF-8')
        suffix = '>%s</%s>' % (escape(data), tag_name)
    else:
        suffix = '/>'
    attrs = collect_attributes(
        tag_name, attributes,
        self_indent,
        self_indent_char,
        len(prefix) + len(suffix))
    return prefix + attrs + suffix


class XMLWriter(object):

    def __init__(self):
        # Build up the XML buffer as unicode strings. When writing to disk,
        # we can assume the lack of a Byte Order Mark (BOM) and lack
        # of an "encoding" xml property means utf-8.
        # See: http://www.opentag.com/xfaq_enc.htm#enc_default
        self._data = StringIO()
        self._data.write('<?xml version="1.0"?>\n')
        self._tag_stack = []
        self._indent = 0
        self._indent_unit = 2
        self.enable_whitespace()

    # Private

    def _open_tag(self, tag_name, attributes=None):
        if attributes is None:
            attributes = []
        attrs = collect_attributes(tag_name, attributes,
                                   self._indent, self._indent_char, len(tag_name) + 2)
        self.write_line('<%s%s>' % (tag_name, attrs))

    def _close_tag(self, tag_name):
        self.write_line('</%s>' % (tag_name, ))

    # Public API

    def enable_whitespace(self):
        self._indent_char = ' '
        self._newline_char = '\n'

    def disable_whitespace(self):
        self._indent_char = ''
        self._newline_char = ''

    def get_xml(self):
        """Returns a unicode string containing the XML."""
        return self._data.getvalue()

    def get_encoded_xml(self):
        """Returns a utf-8 encoded bytes object containing the XML."""
        return self._data.getvalue().encode('utf-8')

    def write_line(self, line='', indent=True, do_escape=False):
        if isinstance(line, bytes):
            line = line.decode('utf-8')
        assert isinstance(line, str)
        if do_escape:
            line = escape(line)
        if indent:
            self._data.write('%s%s%s' % (self._indent_char * self._indent,
                                         line,
                                         self._newline_char))
        else:
            self._data.write('%s%s' % (line, self._newline_char))

    def write_comment(self, text):
        self.write_line('<!-- %s -->' % (text, ))

    def write_tag(self, tag_name, attributes, data=None):
        self.write_line(build_xml_tag(tag_name, attributes, data,
                                      self._indent, self._indent_char))

    def push_tag(self, tag_name, attributes=None):
        if attributes is None:
            attributes = []
        self._open_tag(tag_name, attributes)
        self._tag_stack.append(tag_name)
        self._indent += self._indent_unit

    def pop_tag(self):
        self._indent -= self._indent_unit
        tag_name = self._tag_stack.pop()
        self._close_tag(tag_name)
        return tag_name

    @contextmanager
    def tagcontext(self, tag_name, attributes=None):
        self.push_tag(tag_name, attributes)
        try:
            yield
        finally:
            self.pop_tag()
