# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2010 Zach Goldberg
# Copyright (C) 2011 Johan Dahlin
# Copyright (C) 2011 Shaun McCance
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
import re
import sys
import tempfile

from xml.sax import saxutils
from mako.lookup import TemplateLookup
import markdown

from . import ast, xmlwriter
from .utils import to_underscores
from .mdextensions import InlineMarkdown

# Freely inspired from
# https://github.com/GNOME/yelp-xsl/blob/master/js/syntax.html
language_mimes = {
    "bash-script": "application/x-shellscript",
    "shell": "application/x-shellscript",
    "csharp": "text/x-csharp",
    "css": "text/css",
    "diff": "text/xpatch",
    "html": "text/html",
    "java": "text/x-java",
    "javascript": "application/javascript",
    "lisp": "text/x-scheme",
    "lua": "text-x-lua",
    "c": "text/x-csrc",
    "c++": "text/x-c++src",
    "pascal": "text/x-pascal",
    "perl": "application/x-perl",
    "php": "application/x-php",
    "plain": "text/plain",
    "python": "text/x-python",
    "ruby": "application/x-ruby",
    "sql": "text/x-sql",
    "yaml": "application/x-yaml",
}


def get_headerid_ext():
    try:
        from markdown.extensions.headerid import HeaderIdExtension
    except ImportError:
        # markdown 3.x
        from markdown.extensions.toc import TocExtension
        return TocExtension(toc_depth=0)
    else:
        # markdown 2.x
        return HeaderIdExtension(forceid=False)


def make_page_id(node, recursive=False):
    if isinstance(node, ast.Namespace):
        if recursive:
            return node.name
        else:
            return 'index'

    if hasattr(node, '_chain') and node._chain:
        parent = node._chain[-1]
    else:
        parent = getattr(node, 'parent', None)

    if parent is None:
        if isinstance(node, ast.Function) and node.shadows:
            return '%s.%s' % (node.namespace.name, node.shadows)
        else:
            return '%s.%s' % (node.namespace.name, node.name)

    if isinstance(node, (ast.Property, ast.Signal, ast.VFunction, ast.Field)):
        return '%s-%s' % (make_page_id(parent, recursive=True), node.name)
    elif isinstance(node, ast.Function) and node.shadows:
        return '%s.%s' % (make_page_id(parent, recursive=True), node.shadows)
    else:
        return '%s.%s' % (make_page_id(parent, recursive=True), node.name)


def get_node_kind(node):
    if isinstance(node, ast.Namespace):
        node_kind = 'namespace'
    elif isinstance(node, (ast.Class, ast.Boxed, ast.Compound)):
        node_kind = 'class'
    elif isinstance(node, ast.Interface):
        node_kind = 'interface'
    elif isinstance(node, ast.Record):
        node_kind = 'record'
    elif isinstance(node, ast.Function):
        if node.is_method:
            node_kind = 'method'
        elif node.is_constructor:
            node_kind = 'constructor'
        else:
            node_kind = 'function'
    elif isinstance(node, (ast.Enum, ast.Bitfield)):
        node_kind = 'enum'
    elif isinstance(node, ast.Property) and node.parent is not None:
        node_kind = 'property'
    elif isinstance(node, ast.Signal) and node.parent is not None:
        node_kind = 'signal'
    elif isinstance(node, ast.VFunction) and node.parent is not None:
        node_kind = 'vfunc'
    elif isinstance(node, ast.Callable):
        node_kind = 'callback'
    elif isinstance(node, ast.Field):
        node_kind = 'field'
    else:
        node_kind = 'default'

    return node_kind


class TemplatedScanner(object):
    def __init__(self, specs):
        self.specs = self.unmangle_specs(specs)
        self.regex = self.make_regex(self.specs)

    def unmangle_specs(self, specs):
        mangled = re.compile('<<([a-zA-Z_:]+)>>')
        specdict = dict((name.lstrip('!'), spec) for name, spec in specs)

        def unmangle(spec, name=None):
            def replace_func(match):
                child_spec_name = match.group(1)

                if ':' in child_spec_name:
                    pattern_name, child_spec_name = child_spec_name.split(':', 1)
                else:
                    pattern_name = None

                child_spec = specdict[child_spec_name]
                # Force all child specs of this one to be unnamed
                unmangled = unmangle(child_spec, None)
                if pattern_name and name:
                    return '(?P<%s_%s>%s)' % (name, pattern_name, unmangled)
                else:
                    return unmangled

            return mangled.sub(replace_func, spec)

        return [(name, unmangle(spec, name)) for name, spec in specs]

    def make_regex(self, specs):
        regex = '|'.join('(?P<%s>%s)' % (name, spec) for name, spec in specs
                         if not name.startswith('!'))
        return re.compile(regex)

    def get_properties(self, name, match):
        groupdict = match.groupdict()
        properties = {name: groupdict.pop(name)}
        name = name + "_"
        for group, value in groupdict.items():
            if group.startswith(name):
                key = group[len(name):]
                properties[key] = value
        return properties

    def scan(self, text):
        pos = 0
        while True:
            match = self.regex.search(text, pos)
            if match is None:
                break

            start = match.start()
            if start > pos:
                yield ('other', text[pos:start], None)

            pos = match.end()
            name = match.lastgroup
            yield (name, match.group(0), self.get_properties(name, match))

        if pos < len(text):
            yield ('other', text[pos:], None)


class DocstringScanner(TemplatedScanner):
    def __init__(self):
        specs = [
            ('!alpha', r'[a-zA-Z0-9_]+'),
            ('!alpha_dash', r'[a-zA-Z0-9_-]+'),
            ('code_start_with_language',
            r'\|\[\<!\-\-\s*language\s*\=\s*\"<<language_name:alpha>>\"\s*\-\-\>'),
            ('code_start', r'\|\['),
            ('code_end', r'\]\|'),
            ('html_code_start', r'<code(.*?)>'),
            ('html_code_end', r'</code>'),
            ('markdown_code_toggle', r'\`'),
            ('markdown_attr_start', r'\{'),
            ('markdown_attr_end', r'\}'),
            ('property', r'#<<type_name:alpha>>:(<<property_name:alpha_dash>>)'),
            ('signal', r'#<<type_name:alpha>>::(<<signal_name:alpha_dash>>)'),
            ('type_name', r'#(<<type_name:alpha>>)'),
            ('enum_value', r'%(<<member_name:alpha>>)'),
            ('parameter', r'@<<param_name:alpha>>'),
            ('function_call', r'<<symbol_name:alpha>>\(\)'),
        ]

        super(DocstringScanner, self).__init__(specs)


class DocFormatter(object):
    def __init__(self, transformer):
        self._transformer = transformer
        self._scanner = DocstringScanner()
        # If we are processing a code block as defined by
        # https://wiki.gnome.org/Projects/GTK%2B/DocumentationSyntax/Markdown
        # we won't insert paragraphs and will respect new lines.
        self._processing_code = False
        self._processing_attr = False

    def escape(self, text):
        return saxutils.escape(text)

    def unescape(self, text):
        return saxutils.unescape(text)

    def should_render_node(self, node):
        if getattr(node, "private", False):
            return False
        # Nodes without namespace are AST bugs really
        # They are used for structs and unions declared
        # inline inside other structs, but they are not
        # even picked up by g-ir-compiler, because they
        # don't create a <type/> element.
        # So just ignore them.
        if isinstance(node, ast.Node) and node.namespace is None:
            return False

        return True

    def format(self, node, doc):
        if doc is None:
            return ''

        result = ''
        for para in doc.split('\n\n'):
            result += '  <p>'
            result += self.format_inline(node, para)
            result += '</p>'

        return result

    def _resolve_type(self, ident):
        try:
            matches = self._transformer.split_ctype_namespaces(ident)
        except ValueError:
            return None
        for namespace, name in matches:
            node = namespace.get(name)
            if node:
                return node
        return None

    def _resolve_symbol(self, symbol):
        try:
            matches = self._transformer.split_csymbol_namespaces(symbol)
        except ValueError:
            return None
        for namespace, name in matches:
            node = namespace.get_by_symbol(symbol)
            if node:
                return node
        return None

    def _find_thing(self, list_, name):
        for item in list_:
            if item.name == name:
                return item
        raise KeyError("Could not find %s" % (name, ))

    def _process_other(self, node, match, props):
        if self._processing_code:
            return match
        return self.escape(match)

    def _process_property(self, node, match, props):
        type_node = self._resolve_type(props['type_name'])
        if type_node is None:
            return match

        try:
            prop = self._find_thing(type_node.properties, props['property_name'])
        except (AttributeError, KeyError):
            return match

        return self.format_xref(prop)

    def _process_signal(self, node, match, props):
        type_node = self._resolve_type(props['type_name'])
        if type_node is None:
            return match

        try:
            signal = self._find_thing(type_node.signals, props['signal_name'])
        except (AttributeError, KeyError):
            return match

        return self.format_xref(signal)

    def _process_type_name(self, node, match, props):
        if self._processing_attr:
            return match

        ident = props['type_name']
        type_ = self._resolve_type(ident)
        plural = False
        if type_ is None:
            singularized = ident.rstrip("s")  # Try to remove plural
            type_ = self._resolve_type(singularized)
            plural = True
            if type_ is None:
                return match

        return self.format_xref(type_, pluralize=plural)

    def _process_enum_value(self, node, match, props):
        member_name = props['member_name']

        try:
            return '<code>%s</code>' % (self.fundamentals[member_name], )
        except KeyError:
            pass

        enum_value = self._resolve_symbol(member_name)
        if enum_value:
            return self.format_xref(enum_value)

        return match

    def _process_parameter(self, node, match, props):
        try:
            parameter = node.get_parameter(props['param_name'])
        except (AttributeError, ValueError):
            return match

        return '<code>%s</code>' % (self.format_parameter_name(node, parameter), )

    def _process_function_call(self, node, match, props):
        func = self._resolve_symbol(props['symbol_name'])
        if func is None:
            return match

        return self.format_xref(func)

    # FIXME: the four spaces after newlines in the following functions are to
    # keep Markdown happy. We pass the documentation string first through this
    # templated scanner, which converts |[ ]| to <pre></pre>. Then in the case
    # of DevDocs output, we pass the resulting string through Markdown; but
    # Markdown will not respect the <pre> element and will treat the code as
    # markup, converting asterisks into <em> etc. Putting four spaces at the
    # start of each line makes Markdown recognize the code as code without
    # affecting the normal HTML output too much.
    #
    # A better solution would be to replace DocstringScanner by Markdown
    # entirely, implementing the custom markup with Markdown extensions.
    #
    # UPDATE: As a temporary fix for code blocks we will convert directly to ``` syntax.
    #
    # NOTES:
    # _process_markdown_code_toggle:
    #     Whenever we encounter ` we need to toggle whether we are escaping text as text inside
    #     inline code blocks is unescaped
    # _process_markdown_attr_(start|end):
    #     Whenever we encounter { or } we must stop parsing type names as curly braces are used for
    #     attributes in GIR files in addition to type declarations.
    # _process_html_code_(start|end):
    #     Whenever we encounter an HTML <code> block we must stop escaping text.
    #
    # TODO: Convert to markdown extensions.

    def _process_markdown_code_toggle(self, node, match, props):
        self._processing_code = not self._processing_code
        return match

    def _process_markdown_attr_start(self, node, match, props):
        if not self._processing_code:
            self._processing_attr = True
        return match

    def _process_markdown_attr_end(self, node, match, props):
        if not self._processing_code:
            self._processing_attr = False
        return match

    def _process_html_code_start(self, node, match, props):
        self._processing_code = True
        return match

    def _process_html_code_end(self, node, match, props):
        self._processing_code = False
        return match

    def _process_code_start(self, node, match, props):
        self._processing_code = True
        return '</p>\n```\n'

    def _process_code_start_with_language(self, node, match, props):
        self._processing_code = True
        try:
            return '</p>\n```' + props["language_name"].lower() + '\n'
        except KeyError:
            return '</p>\n```\n'

    def _process_code_end(self, node, match, props):
        self._processing_code = False
        return '\n```\n<p>'

    def _process_new_line(self, node, match, props):
        if self._processing_code:
            return '\n'
        return '\n'

    def _process_new_paragraph(self, node, match, props):
        if self._processing_code:
            return '\n\n'
        return "</p><p>"

    def _process_token(self, node, tok):
        kind, match, props = tok

        dispatch = {
            'other': self._process_other,
            'property': self._process_property,
            'signal': self._process_signal,
            'type_name': self._process_type_name,
            'enum_value': self._process_enum_value,
            'parameter': self._process_parameter,
            'function_call': self._process_function_call,
            'code_start': self._process_code_start,
            'code_start_with_language': self._process_code_start_with_language,
            'code_end': self._process_code_end,
            'html_code_start': self._process_html_code_start,
            'html_code_end': self._process_html_code_end,
            'markdown_code_toggle': self._process_markdown_code_toggle,
            'markdown_attr_start': self._process_markdown_attr_start,
            'markdown_attr_end': self._process_markdown_attr_end,
            'new_line': self._process_new_line,
            'new_paragraph': self._process_new_paragraph,
        }

        return dispatch[kind](node, match, props)

    def get_in_parameters(self, node):
        raise NotImplementedError

    def format_inline(self, node, para):
        tokens = self._scanner.scan(para)
        words = [self._process_token(node, tok) for tok in tokens]
        return ''.join(words)

    def format_parameter_name(self, node, parameter):
        if isinstance(parameter.type, ast.Varargs):
            return "..."
        else:
            return parameter.argname

    def format_function_name(self, func):
        raise NotImplementedError

    def format_type(self, type_, link=False):
        raise NotImplementedError

    def format_value(self, node):
        raise NotImplementedError

    def format_page_name(self, node):
        if isinstance(node, ast.Namespace):
            return node.name
        elif isinstance(node, ast.Function):
            return self.format_function_name(node)
        elif isinstance(node, ast.Property) and node.parent is not None:
            return '%s:%s' % (self.format_page_name(node.parent), node.name)
        elif isinstance(node, ast.Signal) and node.parent is not None:
            return '%s::%s' % (self.format_page_name(node.parent), node.name)
        elif isinstance(node, ast.VFunction) and node.parent is not None:
            return '%s::%s' % (self.format_page_name(node.parent), node.name)
        elif isinstance(node, ast.Field) and node.parent is not None:
            return '%s->%s' % (self.format_page_name(node.parent), node.name)
        else:
            return make_page_id(node)

    def format_xref(self, node, pluralize=False, **attrdict):
        if node is None or not hasattr(node, 'namespace'):
            attrs = [('xref', 'index')] + list(sorted(attrdict.items()))
            return xmlwriter.build_xml_tag('link', attrs)
        elif isinstance(node, ast.Member):
            # Enum/BitField members are linked to the main enum page.
            return self.format_xref(node.parent, pluralize=pluralize, **attrdict) + '.' + node.name
        elif node.namespace is self._transformer.namespace:
            return self.format_internal_xref(node, attrdict, pluralize=pluralize)
        else:
            return self.format_external_xref(node, attrdict, pluralize=pluralize)

    def format_internal_xref(self, node, attrdict, pluralize=False):
        attrs = [('xref', make_page_id(node))] + list(sorted(attrdict.items()))
        if not pluralize:
            return xmlwriter.build_xml_tag('link', attrs)
        else:
            return xmlwriter.build_xml_tag('link', attrs, make_page_id(node) +
            "s")

    def format_external_xref(self, node, attrdict, pluralize=False):
        ns = node.namespace
        attrs = [('href', '../%s-%s/%s.html' % (ns.name, str(ns.version),
                                                make_page_id(node)))]
        attrs += list(sorted(attrdict.items()))
        if not pluralize:
            return xmlwriter.build_xml_tag('link', attrs, self.format_page_name(node))
        else:
            return xmlwriter.build_xml_tag('link', attrs,
                    self.format_page_name(node) + "s")

    def field_is_writable(self, field):
        return True

    def format_property_flags(self, property_, construct_only=False, abbrev=False):
        flags = []

        if property_.readable and not construct_only:
            flags.append("Read")
        if property_.writable and not construct_only and \
           self.field_is_writable(property_):
            flags.append("Write")
        if isinstance(property_, ast.Property):
            if property_.construct:
                flags.append("Construct")
            if property_.construct_only:
                flags.append("Construct Only")

        if abbrev:
            return "/".join([''.join([word[0] for word in flag.lower().split()])
                for flag in flags])
        return " / ".join(flags)

    def format_signal_flags(self, signal):
        flags = []
        if signal.action:
            flags.append("Action")
        if signal.detailed:
            flags.append("Detailed")
        if signal.no_hooks:
            flags.append("No Hooks")
        if signal.no_recurse:
            flags.append("No Recurse")
        if signal.when:
            flags.append("Run " + signal.when.capitalize())
        return " / ".join(flags)

    def to_underscores(self, node):
        if isinstance(node, ast.Property):
            return node.name.replace('-', '_')
        elif node.name:
            return to_underscores(node.name)
        elif isinstance(node, ast.Function) and node.moved_to:
            return to_underscores(node.moved_to)
        elif isinstance(node, ast.Callback):
            return 'callback'
        elif isinstance(node, ast.Union):
            return 'anonymous_union'
        elif isinstance(node, ast.Field):
            return 'anonymous field'
        else:
            raise Exception('invalid node')

    def to_lower_camel_case(self, string):
        return string[0].lower() + string[1:]

    def get_class_hierarchy(self, node):
        assert isinstance(node, ast.Class)

        parent_chain = [node]
        while node.parent_type:
            node = self._transformer.lookup_typenode(node.parent_type)
            parent_chain.append(node)

        parent_chain.reverse()
        return parent_chain

    def get_inheritable_types(self, node):
        """Return an ast.Node object for each type (ast.Class and ast.Interface
        types) from which an ast.Class @node might inherit methods, properties,
        and signals."""

        assert isinstance(node, ast.Class)

        parent_chain = self.get_class_hierarchy(node)
        types = []
        for p in parent_chain:
            types += [self._transformer.lookup_typenode(t) for t in p.interfaces]
        types += [t for t in parent_chain if t is not node]
        return types

    def is_private_field(self, node, f):
        """Returns whether @f is a private field of @node (including a heuristic
        that tries to determine whether the field is the parent instance field
        or a private pointer but not marked as such.)"""

        if f.private:
            return True
        if f.anonymous_node:
            return True
        if f.name == 'g_type_instance':
            return True  # this field on GObject is not exposed

        field_typenode = self._transformer.lookup_typenode(f.type)
        if not field_typenode:
            return False

        if getattr(field_typenode, 'disguised', False):
            return True  # guess that it's a pointer to a private struct
            # this also catches fields of type GdkAtom, since that is disguised
            # as well. Not sure whether that's correct or not.

        if not isinstance(node, ast.Class):
            return False  # parent instance heuristics only apply to classes

        if node.parent_type:
            parent_typenode = self._transformer.lookup_typenode(node.parent_type)
            if field_typenode == parent_typenode:
                return True  # guess that it's a parent instance field

        return False

    def format_prerequisites(self, node):
        assert isinstance(node, ast.Interface)

        if len(node.prerequisites) > 0:
            if len(node.prerequisites) > 1:
                return ', '.join(node.prerequisites[:-1]) + \
                    ' and ' + node.prerequisites[-1]
            else:
                return node.prerequisites[0]
        else:
            return 'GObject.Object'

    def format_known_implementations(self, node):
        assert isinstance(node, ast.Interface)

        node_name = node.namespace.name + '.' + node.name
        impl = []

        for c in node.namespace.values():
            if not isinstance(c, ast.Class):
                continue
            for implemented in c.interfaces:
                if implemented.target_giname == node_name:
                    impl.append(c)
                    break

        if len(impl) == 0:
            return 'None'
        else:
            out = '%s is implemented by ' % (node.name,)
            if len(impl) == 1:
                return out + impl[0].name
            else:
                return out + ', '.join(i.name for i in impl[:-1]) + \
                    ' and ' + impl[-1].name


class DocFormatterC(DocFormatter):
    language = "C"
    mime_type = "text/x-csrc"
    output_format = "mallard"
    output_extension = ".page"

    fundamentals = {
        "TRUE": "TRUE",
        "FALSE": "FALSE",
        "NULL": "NULL",
    }

    def format_type(self, type_, link=False):
        if isinstance(type_, ast.Array):
            return self.format_type(type_.element_type) + '*'
        elif type_.ctype is not None:
            return type_.ctype
        elif type_.target_fundamental:
            return type_.target_fundamental
        else:
            node = self._transformer.lookup_typenode(type_)
            return getattr(node, 'ctype')

    def format_function_name(self, func):
        if isinstance(func, ast.Function):
            return func.symbol
        else:
            return func.name

    def get_in_parameters(self, node):
        return node.all_parameters


class DocFormatterIntrospectableBase(DocFormatter):
    def should_render_node(self, node):
        if isinstance(node, ast.Record) and node.is_gtype_struct_for is not None:
            return False

        if not getattr(node, "introspectable", True):
            return False

        if isinstance(node, ast.Function) and node.shadowed_by is not None:
            return False

        return super(DocFormatterIntrospectableBase, self).should_render_node(node)


class DocFormatterPython(DocFormatterIntrospectableBase):
    language = "Python"
    mime_type = "text/python"
    output_format = "mallard"
    output_extension = ".page"

    fundamentals = {
        "TRUE": "True",
        "FALSE": "False",
        "NULL": "None",
    }

    def get_output_format(self):
        return 'mallard'

    def get_output_extension(self):
        return 'page'

    def should_render_node(self, node):
        if getattr(node, "is_constructor", False):
            return False

        return super(DocFormatterPython, self).should_render_node(node)

    def is_method(self, node):
        if getattr(node, "is_method", False):
            return True

        if isinstance(node, ast.VFunction):
            return True

        return False

    def format_parameter_name(self, node, parameter):
        # Force "self" for the first parameter of a method
        if self.is_method(node) and parameter is node.instance_parameter:
            return "self"
        elif isinstance(parameter.type, ast.Varargs):
            return "..."
        else:
            return parameter.argname

    def format_fundamental_type(self, name):
        fundamental_types = {
            "utf8": "unicode",
            "gunichar": "unicode",
            "gchar": "str",
            "guchar": "str",
            "gboolean": "bool",
            "gint": "int",
            "guint": "int",
            "glong": "int",
            "gulong": "int",
            "gint64": "int",
            "guint64": "int",
            "time_t": "int",
            "off_t": "int",
            "gfloat": "float",
            "gdouble": "float",
            "gchararray": "str",
            "GParam": "GLib.Param",
            "PyObject": "object",
            "GStrv": "[str]",
            "GVariant": "GLib.Variant"}

        return fundamental_types.get(name, name)

    def format_type(self, type_, link=False):
        if isinstance(type_, (ast.List, ast.Array)):
            return '[' + self.format_type(type_.element_type) + ']'
        elif isinstance(type_, ast.Map):
            return '{%s: %s}' % (self.format_type(type_.key_type),
                                 self.format_type(type_.value_type))
        elif type_.target_giname is not None:
            return type_.target_giname
        else:
            return self.format_fundamental_type(type_.target_fundamental)

    def format_function_name(self, func):
        if func.parent is not None:
            return "%s.%s" % (self.format_page_name(func.parent), func.name)
        else:
            return func.name

    def get_in_parameters(self, node):
        return node.all_parameters


class DocFormatterGjs(DocFormatterIntrospectableBase):
    language = "Gjs"
    mime_type = "text/x-gjs"
    output_format = "mallard"
    output_extension = ".page"

    fundamentals = {
        "TRUE": "true",
        "FALSE": "false",
        "NULL": "null",
    }

    def is_method(self, node):
        if getattr(node, "is_method", False):
            return True

        if isinstance(node, ast.VFunction):
            return True

        return False

    def resolve_gboxed_constructor(self, node):
        zero_args_constructor = None
        default_constructor = None

        introspectable_constructors = \
            list(filter(lambda c: getattr(c, 'introspectable', True),
                   node.constructors))
        for c in introspectable_constructors:
            if zero_args_constructor is None and \
               len(c.parameters) == 0:
                zero_args_constructor = c
            if default_constructor is None and \
               c.name == 'new':
                default_constructor = c
        if default_constructor is None:
            default_constructor = zero_args_constructor
        if default_constructor is None and \
           len(introspectable_constructors) > 0:
            default_constructor = introspectable_constructors[0]

        node.gjs_default_constructor = default_constructor
        node.gjs_zero_args_constructor = zero_args_constructor

    def should_render_node(self, node):
        if isinstance(node, (ast.Compound, ast.Boxed)):
            self.resolve_gboxed_constructor(node)

        if isinstance(node, ast.Compound) and node.disguised and \
           len(node.methods) == len(node.static_methods) == len(node.constructors) == 0:
            return False

        if isinstance(node, ast.ErrorQuarkFunction):
            return False
        if isinstance(node, ast.Field):
            if node.type is None:
                return False
            if isinstance(node.parent, (ast.Class, ast.Union)):
                return False
        if isinstance(node, ast.Union) and node.name is None:
            return False
        if isinstance(node, ast.Class):
            is_gparam_subclass = False
            if node.parent_type:
                parent = self._transformer.lookup_typenode(node.parent_type)
                while parent:
                    if parent.namespace.name == 'GObject' and \
                       parent.name == 'ParamSpec':
                        is_gparam_subclass = True
                        break
                    if parent.parent_type is None:
                        break
                    parent = self._transformer.lookup_typenode(parent.parent_type)
            if is_gparam_subclass:
                return False

        return super(DocFormatterGjs, self).should_render_node(node)

    def format_fundamental_type(self, name):
        fundamental_types = {
            "none": "void",
            "gpointer": "void",
            "gboolean": "Boolean",
            "gint8": "Number(gint8)",
            "guint8": "Number(guint8)",
            "gint16": "Number(gint16)",
            "guint16": "Number(guint16)",
            "gint32": "Number(gint32)",
            "guint32": "Number(guint32)",
            "gchar": "Number(gchar)",
            "guchar": "Number(guchar)",
            "gshort": "Number(gshort)",
            "gint": "Number(gint)",
            "guint": "Number(guint)",
            "gfloat": "Number(gfloat)",
            "gdouble": "Number(gdouble)",
            "utf8": "String",
            "gunichar": "String",
            "filename": "String",
            "GType": "GObject.Type",
            "GVariant": "GLib.Variant",
            # These cannot be fully represented in gjs
            "gsize": "Number(gsize)",
            "gssize": "Number(gssize)",
            "gintptr": "Number(gintptr)",
            "guintptr": "Number(guintptr)",
            "glong": "Number(glong)",
            "gulong": "Number(gulong)",
            "gint64": "Number(gint64)",
            "guint64": "Number(guint64)",
            "time_t": "Number(time_t)",
            "off_t": "Number(off_t)",
            "long double": "Number(long double)",
            "long long": "Number(long long)",
            "unsigned long long": "Number(unsigned long long)"}

        return fundamental_types.get(name, name)

    def format_type(self, type_, link=False):
        if isinstance(type_, ast.Array) and \
           type_.element_type.target_fundamental in ('gint8', 'guint8'):
            return 'ByteArray'
        elif isinstance(type_, (ast.List, ast.Array)):
            return 'Array(' + self.format_type(type_.element_type, link) + ')'
        elif isinstance(type_, ast.Map):
            return '{%s: %s}' % (self.format_type(type_.key_type, link),
                                 self.format_type(type_.value_type, link))
        elif not type_ or type_.target_fundamental == "none":
            return "void"
        elif type_.target_giname is not None:
            giname = type_.target_giname
            if giname == 'Gdk.Atom':
                return 'String'
            if giname in ('GLib.ByteArray', 'GLib.Bytes'):
                return 'Uint8Array'
            if giname == 'GObject.Value':
                return 'Any'
            if giname == 'GObject.Closure':
                return 'Function'
            if link:
                nsname = self._transformer.namespace.name
                if giname.startswith(nsname + '.'):
                    return '<link xref="%s">%s</link>' % (giname, giname)
                else:
                    resolved = self._transformer.lookup_typenode(type_)
                    if resolved:
                        return self.format_xref(resolved)
            return giname
        else:
            return self.format_fundamental_type(type_.target_fundamental)

    def format_function_name(self, func):
        name = func.name
        if func.shadows:
            name = func.shadows

        if func.is_method:
            return "%s.prototype.%s" % (self.format_page_name(func.parent), name)
        elif func.parent is not None:
            return "%s.%s" % (self.format_page_name(func.parent), name)
        else:
            return name

    def format_page_name(self, node):
        if isinstance(node, (ast.Field, ast.Property)):
            return '%s.%s' % (self.format_page_name(node.parent), self.to_underscores(node))
        else:
            return DocFormatterIntrospectableBase.format_page_name(self, node)

    def has_any_parameters(self, node):
        return len(node.parameters) > 0 or \
            node.retval.type.target_fundamental != 'none'

    def get_in_parameters(self, node):
        skip = set()
        for param in node.parameters:
            if param.direction == ast.PARAM_DIRECTION_OUT:
                skip.add(param)
            if param.closure_name is not None:
                skip.add(node.get_parameter(param.closure_name))
            if param.destroy_name is not None:
                skip.add(node.get_parameter(param.destroy_name))
            if isinstance(param.type, ast.Array) and param.type.length_param_name is not None:
                skip.add(node.get_parameter(param.type.length_param_name))

        params = []
        for param in node.parameters:
            if param not in skip:
                params.append(param)
        return params

    def get_out_parameters(self, node):
        skip = set()
        for param in node.parameters:
            if param.direction == ast.PARAM_DIRECTION_IN:
                skip.add(param)
            if param.closure_name is not None:
                skip.add(node.get_parameter(param.closure_name))
            if param.destroy_name is not None:
                skip.add(node.get_parameter(param.destroy_name))
            if isinstance(param.type, ast.Array) and param.type.length_param_name is not None:
                skip.add(node.get_parameter(param.type.length_param_name))

        params = []
        if node.retval.type.target_fundamental != 'none':
            name = 'return_value'
            if node.retval.type.target_fundamental == 'gboolean':
                name = 'ok'

            ret_param = ast.Parameter(name, node.retval.type,
                                      ast.PARAM_DIRECTION_OUT)
            ret_param.doc = node.retval.doc
            params.append(ret_param)
        for param in node.parameters:
            if param not in skip:
                params.append(param)

        if len(params) == 1:
            params[0].argname = 'Returns'

        return params

    def format_in_parameters(self, node):
        in_params = self.get_in_parameters(node)
        return ', '.join(('%s: %s' % (p.argname, self.format_type(p.type, True)))
                         for p in in_params)

    def format_out_parameters(self, node):
        out_params = self.get_out_parameters(node)

        if len(out_params) == 0:
            return 'void'
        elif len(out_params) == 1:
            return self.format_type(out_params[0].type, True)
        else:
            return '[' + ', '.join(('%s: %s' % (p.argname, self.format_type(p.type, True)))
                                   for p in out_params) + ']'

    def field_is_writable(self, node):
        if isinstance(node, ast.Field):
            if node.type is None:
                return False
            if node.private:
                return False
            if isinstance(node.parent, ast.Union):
                return False
            if node.type.target_fundamental not in \
               (None, 'none', 'gpointer', 'utf8', 'filename', 'va_list'):
                return True

            resolved = self._transformer.lookup_typenode(node.type)
            if resolved:
                if isinstance(resolved, ast.Compound) and node.type.ctype[-1] != '*':
                    return self._struct_is_simple(resolved)
                elif isinstance(resolved, (ast.Enum, ast.Bitfield)):
                    return True
            return False
        else:
            return True

    def _struct_is_simple(self, node):
        if node.disguised or len(node.fields) == 0:
            return False

        for f in node.fields:
            if not self.field_is_writable(f):
                return False
        return True

    def format_gboxed_constructor(self, node):
        if node.namespace.name == 'GLib' and node.name == 'Variant':
            return 'signature: String, value: Any'

        zero_args_constructor = node.gjs_zero_args_constructor
        default_constructor = node.gjs_default_constructor

        can_allocate = zero_args_constructor is not None
        if not can_allocate and isinstance(node, ast.Record):
            can_allocate = self._struct_is_simple(node)

        # Small lie: if can_allocate is False, and
        # default_constructor is None, then you cannot
        # construct the boxed in any way. But let's
        # pretend you can with the regular constructor
        if can_allocate or default_constructor is None:
            if isinstance(node, ast.Compound):
                fields = filter(self.field_is_writable, node.fields)
                out = ''
                for f in fields:
                    out += "    <link xref='%s.%s-%s'>%s</link>: value\n" % \
                           (node.namespace.name, node.name, f.name, f.name)
                if out:
                    out = "{\n" + out + "}"
                return out
            else:
                return ''
        else:
            construct_params = self.get_in_parameters(default_constructor)
            return ', '.join(('%s: %s' % (p.argname, self.format_type(p.type)))
                             for p in construct_params)


class DevDocsFormatterGjs(DocFormatterGjs):
    output_format = "devdocs"
    output_extension = ".html"

    def _is_static_method(self, node):
        if not hasattr(node.parent, "static_methods"):
            return False
        return node in node.parent.static_methods

    def should_render_node(self, node):
        # For DevDocs, we only want to render the top-level nodes.
        if isinstance(node, (ast.Compound, ast.Boxed)):
            self.resolve_gboxed_constructor(node)

        if not super(DevDocsFormatterGjs, self).should_render_node(node):
            return False

        if isinstance(node, ast.Function) and not node.is_method and \
           not node.is_constructor and not self._is_static_method(node):
            return True  # module-level function
        toplevel_types = [ast.Alias, ast.Bitfield, ast.Boxed, ast.Callback,
            ast.Class, ast.Constant, ast.Enum, ast.Interface, ast.Namespace,
            ast.Record, ast.Union]
        for ast_type in toplevel_types:
            if isinstance(node, ast_type):
                return True

        return False

    def format_fundamental_type(self, name):
        # Don't specify the C type after Number as the Mallard docs do; it's
        # confusing to GJS newbies.
        if name in ["gint8", "guint8", "gint16", "guint16", "gint32", "guint32",
                    "gchar", "guchar", "gshort", "gint", "guint", "gfloat",
                    "gdouble", "gsize", "gssize", "gintptr", "guintptr",
                    "glong", "gulong", "gint64", "guint64", "long double",
                    "long long", "unsigned long long", "time_t", "off_t"]:
            return "Number"  # gsize and up cannot fully be represented in GJS
        if name in ["none", "gpointer"]:
            return "void"
        if name in ["utf8", "gunichar", "filename"]:
            return "String"
        if name == "gboolean":
            return "Boolean"
        if name == "GType":
            return "GObject.Type"
        if name == "GVariant":
            return "GLib.Variant"
        return name

    def format_value(self, node):
        # Constants only have fundamental types?
        type_ = node.value_type.target_fundamental
        if type_ in ["utf8", "gunichar", "filename"]:
            return repr(node.value)
            # escapes quotes in the string; ought to be the same in Javascript
        return node.value

    def format(self, node, doc):
        if doc is None:
            return ''

        cleaned_up_gtkdoc = super(DevDocsFormatterGjs, self).format_inline(node, doc)
        return markdown.markdown(cleaned_up_gtkdoc, extensions=[
            'markdown.extensions.fenced_code',
            'markdown.extensions.nl2br',
            'markdown.extensions.attr_list',
            get_headerid_ext(),
        ])

    def format_function_name(self, func):
        name = func.name
        if func.shadows:
            name = func.shadows

        if isinstance(func, ast.VFunction):
            return 'vfunc_' + name
        return name

    def format_page_name(self, node):
        if isinstance(node, ast.Function) and node.parent is not None:
            return node.parent.name + "." + self.format_function_name(node)
        return super(DevDocsFormatterGjs, self).format_page_name(node)

    def _write_xref_markdown(self, target, anchor=None, display_name=None, pluralize=False):
        if display_name is None:
            display_name = target
        link = target + ".html"
        if anchor is not None:
            link += "#" + anchor
        return "[{}]({}){}".format(display_name, link, 's' if pluralize else '')

    def to_underscores(self, node):
        try:
            return super(DevDocsFormatterGjs, self).to_underscores(node)
        except Exception as e:
            if e.args[0] == 'invalid node':
                print('warning: invalid node in', node.parent.name,
                    file=sys.stderr)
                return node.parent.name + '_invalid_node'

    def make_anchor(self, node):
        style_class = get_node_kind(node)
        return "{}-{}".format(style_class, self.to_underscores(node))

    def _process_parameter(self, node, match, props):
        # Display the instance parameter as "this" instead of whatever name it
        # has in C.
        if hasattr(node, 'instance_parameter') and \
           node.instance_parameter is not None and \
           props['param_name'] == node.instance_parameter.argname:
            return '<code>this</code>'
        return super(DevDocsFormatterGjs, self)._process_parameter(node, match, props)

    def format_xref(self, node, pluralize=False, **attrdict):
        if node is None or not hasattr(node, 'namespace'):
            return self._write_xref_markdown('index')
        if node.namespace is self._transformer.namespace:
            return self.format_internal_xref(node, attrdict, pluralize=pluralize)
        return self.format_external_xref(node, attrdict, pluralize=pluralize)

    def format_internal_xref(self, node, attrdict, pluralize=False):
        if not self.should_render_node(node):
            # Non-toplevel nodes are linked to the main page.
            page = make_page_id(node.parent)
            name = node.name
            if isinstance(node, ast.Member):
                name = name.upper()
            return self._write_xref_markdown(page, self.make_anchor(node),
                                             page + "." + name,
                                             pluralize=pluralize)
        return self._write_xref_markdown(make_page_id(node), pluralize=pluralize)

    def format_external_xref(self, node, attrdict, pluralize=False):
        ns = node.namespace
        slug = ns.name.lower() + str(ns.version).replace('.', '')
        if not self.should_render_node(node):
            target = 'gir:///%s/%s' % (slug, make_page_id(node.parent))
            return self._write_xref_markdown(target, self.make_anchor(node),
                                             self.format_page_name(node.parent),
                                             pluralize=pluralize)
        target = 'gir:///%s/%s' % (slug, make_page_id(node))
        return self._write_xref_markdown(target, None,
                                         self.format_page_name(node),
                                         pluralize=pluralize)

    def format_inline(self, node, para):
        if para is None:
            return ''
        cleaned_up_gtkdoc = super(DevDocsFormatterGjs, self).format_inline(node, para)
        return markdown.markdown(cleaned_up_gtkdoc, extensions=[
            InlineMarkdown(),
            'markdown.extensions.fenced_code',
            'markdown.extensions.nl2br',
            'markdown.extensions.attr_list',
            get_headerid_ext(),
        ])

    def format_in_parameters(self, node):
        return ', '.join(p.argname for p in self.get_in_parameters(node))

    def format_signal_parameters(self, node):
        emitter = self.to_lower_camel_case(node.parent.name)
        in_params = self.format_in_parameters(node)

        return '%s, %s' % (emitter, in_params) if in_params else emitter


LANGUAGES = {
    "devdocs": {
        "gjs": DevDocsFormatterGjs,
    },
    "mallard": {
        "c": DocFormatterC,
        "python": DocFormatterPython,
        "gjs": DocFormatterGjs,
    },
}


class DocWriter(object):
    def __init__(self, transformer, language, output_format, templates_dir=None):
        self._transformer = transformer

        try:
            formatter_class = LANGUAGES[output_format][language.lower()]
        except KeyError:
            raise SystemExit("Unsupported language %s for output format %s" %
                             (language, output_format))

        self._formatter = formatter_class(self._transformer)
        self._language = self._formatter.language
        self._output_format = output_format
        self._templates_dir = templates_dir

        self._lookup = self._get_template_lookup()

    def _get_template_lookup(self):
        if self._templates_dir is not None:
            srcdir = self._templates_dir
        elif 'UNINSTALLED_INTROSPECTION_SRCDIR' in os.environ:
            top_srcdir = os.environ['UNINSTALLED_INTROSPECTION_SRCDIR']
            srcdir = os.path.join(top_srcdir, 'giscanner', 'doctemplates')
        else:
            srcdir = os.path.join(os.path.dirname(__file__), 'doctemplates')

        template_dir = os.path.join(srcdir, self._formatter.output_format)

        return TemplateLookup(directories=[template_dir],
                              module_directory=tempfile.mkdtemp(),
                              output_encoding='utf-8')

    def write(self, output):
        try:
            os.makedirs(output)
        except OSError:
            # directory already made
            pass

        self._walk_node(output, self._transformer.namespace, [])
        self._transformer.namespace.walk(lambda node, chain: self._walk_node(output, node, chain))

    def _walk_node(self, output, node, chain):
        if isinstance(node, ast.Function) and node.moved_to is not None:
            return False
        if self._formatter.should_render_node(node):
            self._render_node(node, chain, output)

            # hack: fields are not Nodes in the ast, so we don't
            # see them in the visit. Handle them manually here
            if isinstance(node, (ast.Compound, ast.Class)):
                chain.append(node)
                for f in node.fields:
                    self._walk_node(output, f, chain)
                chain.pop()
            return True
        return False

    def _render_node(self, node, chain, output):
        namespace = self._transformer.namespace

        # A bit of a hack...maybe this should be an official API
        node._chain = list(chain)

        page_kind = get_node_kind(node)
        template_name = '%s/%s.tmpl' % (self._language, page_kind)
        page_id = make_page_id(node)

        template = self._lookup.get_template(template_name)
        result = template.render(namespace=namespace,
                                 node=node,
                                 page_id=page_id,
                                 page_kind=page_kind,
                                 get_node_kind=get_node_kind,
                                 formatter=self._formatter,
                                 ast=ast)

        output_base_name = page_id + self._formatter.output_extension
        output_file_name = os.path.join(os.path.abspath(output),
                                        output_base_name)
        with open(output_file_name, 'wb') as fp:
            fp.write(result)
