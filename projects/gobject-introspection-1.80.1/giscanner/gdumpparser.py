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

import os
import sys
import tempfile
import subprocess
from xml.etree.ElementTree import parse

from . import ast
from . import message
from . import utils
from .transformer import TransformerException
from .utils import to_underscores

# GParamFlags
G_PARAM_READABLE = 1 << 0
G_PARAM_WRITABLE = 1 << 1
G_PARAM_CONSTRUCT = 1 << 2
G_PARAM_CONSTRUCT_ONLY = 1 << 3
G_PARAM_LAX_VALIDATION = 1 << 4
G_PARAM_STATIC_NAME = 1 << 5
G_PARAM_STATIC_NICK = 1 << 6
G_PARAM_STATIC_BLURB = 1 << 7


class IntrospectionBinary(object):

    def __init__(self, args, tmpdir=None):
        self.args = args
        if tmpdir is None:
            self.tmpdir = tempfile.mkdtemp('', 'tmp-introspect')
        else:
            self.tmpdir = tmpdir


class Unresolved(object):

    def __init__(self, target):
        self.target = target


class UnknownTypeError(Exception):
    pass


class GDumpParser(object):

    def __init__(self, transformer):
        self._transformer = transformer
        self._namespace = transformer.namespace
        self._binary = None
        self._get_type_functions = []
        self._error_quark_functions = []
        self._error_domains = {}
        self._boxed_types = {}
        self._private_internal_types = {}

    # Public API

    def init_parse(self):
        """Do parsing steps that don't involve the introspection binary

        This does enough work that get_type_functions() can be called.

        """

        # First pass: parsing
        for node in list(self._namespace.values()):
            if isinstance(node, ast.Function):
                self._initparse_function(node)

        if self._namespace.name == 'GObject' or self._namespace.name == 'GLib':
            for node in list(self._namespace.values()):
                if isinstance(node, ast.Record):
                    self._initparse_gobject_record(node)

    def get_get_type_functions(self):
        return self._get_type_functions

    def get_error_quark_functions(self):
        return self._error_quark_functions

    def set_introspection_binary(self, binary):
        self._binary = binary

    def parse(self):
        """Do remaining parsing steps requiring introspection binary"""

        # Get all the GObject data by passing our list of get_type
        # functions to the compiled binary, returning an XML blob.
        tree = self._execute_binary_get_tree()
        root = tree.getroot()
        for child in root:
            if child.tag == 'error-quark':
                self._introspect_error_quark(child)
            else:
                self._introspect_type(child)

        # Pair up boxed types and class records
        for name, boxed in self._boxed_types.items():
            self._pair_boxed_type(boxed)
        for node in list(self._namespace.values()):
            if isinstance(node, (ast.Class, ast.Interface)):
                self._find_class_record(node)

        # Clear the _get_type functions out of the namespace;
        # Anyone who wants them can get them from the ast.Class/Interface/Boxed
        to_remove = []
        for name, node in self._namespace.items():
            if isinstance(node, ast.Registered) and node.get_type is not None:
                get_type_name = node.get_type
                if get_type_name == 'intern':
                    continue
                assert get_type_name, node
                (ns, name) = self._transformer.split_csymbol(get_type_name)
                assert ns is self._namespace
                get_type_func = self._namespace.get(name)
                assert get_type_func, name
                to_remove.append(get_type_func)
        for node in to_remove:
            self._namespace.remove(node)

    # Helper functions

    def _execute_binary_get_tree(self):
        """Load the library (or executable), returning an XML
blob containing data gleaned from GObject's primitive introspection."""
        in_path = os.path.join(self._binary.tmpdir, 'functions.txt')
        with open(in_path, 'w', encoding='utf-8') as f:
            for func in self._get_type_functions:
                f.write('get-type:')
                f.write(func)
                f.write('\n')
            for func in self._error_quark_functions:
                f.write('error-quark:')
                f.write(func)
                f.write('\n')
        out_path = os.path.join(self._binary.tmpdir, 'dump.xml')

        args = []

        # Prepend the launcher command and arguments, if defined
        launcher = os.environ.get('GI_CROSS_LAUNCHER')
        if launcher:
            args.extend(launcher.split())

        args.extend(self._binary.args)
        args.append('--introspect-dump=%s,%s' % (in_path, out_path))

        # Invoke the binary, having written our get_type functions to types.txt
        try:
            try:
                subprocess.check_call(args, stdout=sys.stdout, stderr=sys.stderr)
            except subprocess.CalledProcessError as e:
                # Clean up temporaries
                raise SystemExit(e)
            return parse(out_path)
        finally:
            if not utils.have_debug_flag('save-temps'):
                utils.rmtree(self._binary.tmpdir)

    # Parser

    def _initparse_function(self, func):
        symbol = func.symbol
        if symbol.startswith('_'):
            return
        elif (symbol.endswith('_get_type') or symbol.endswith('_get_gtype')):
            self._initparse_get_type_function(func)
        elif symbol.endswith('_error_quark'):
            self._initparse_error_quark_function(func)

    def _initparse_get_type_function(self, func):
        if func.symbol == 'g_variant_get_gtype':
            # We handle variants internally, see _initparse_gobject_record
            return True

        if func.is_type_meta_function():
            self._get_type_functions.append(func.symbol)
            return True

        return False

    def _initparse_error_quark_function(self, func):
        if (func.retval.type.ctype != 'GQuark'):
            return False
        self._error_quark_functions.append(func.symbol)
        return True

    def _initparse_gobject_record(self, record):
        if (record.name.startswith('ParamSpec')
        and record.name not in ('ParamSpecPool', 'ParamSpecClass', 'ParamSpecTypeInfo')):
            parent = None
            if record.name != 'ParamSpec':
                parent = ast.Type(target_giname='GObject.ParamSpec')
            prefix = to_underscores(record.name).lower()
            node = ast.Class(record.name, parent,
                             ctype=record.ctype,
                             # GParamSpecXxx has g_type_name 'GParamXxx'
                             gtype_name=record.ctype.replace('Spec', ''),
                             get_type='intern',
                             c_symbol_prefix=prefix)
            node.fundamental = True
            if record.name == 'ParamSpec':
                node.is_abstract = True
            self._add_record_fields(node)
            self._namespace.append(node, replace=True)
        elif record.name == 'Variant':
            self._boxed_types['GVariant'] = ast.Boxed('Variant',
                                                      gtype_name='GVariant',
                                                      get_type='intern',
                                                      c_symbol_prefix='variant')
        elif record.name == 'InitiallyUnownedClass':
            # InitiallyUnowned is just GObject with extra steps, so we alias
            # it in the introspection data
            record.fields = self._namespace.get('ObjectClass').fields
            record.opaque = False
            record.disguised = False

    # Introspection over the data we get from the dynamic
    # GObject/GType system out of the binary

    def _introspect_type(self, xmlnode):
        if xmlnode.tag in ('enum', 'flags'):
            self._introspect_enum(xmlnode)
        elif xmlnode.tag == 'class':
            self._introspect_object(xmlnode)
        elif xmlnode.tag == 'interface':
            self._introspect_interface(xmlnode)
        elif xmlnode.tag == 'boxed':
            self._introspect_boxed(xmlnode)
        elif xmlnode.tag == 'fundamental':
            self._introspect_fundamental(xmlnode)
        else:
            raise ValueError("Unhandled introspection XML tag %s", xmlnode.tag)

    def _introspect_enum(self, xmlnode):
        type_name = xmlnode.attrib['name']
        (get_type, c_symbol_prefix) = self._split_type_and_symbol_prefix(xmlnode)
        try:
            enum_name = self._transformer.strip_identifier(type_name)
        except TransformerException as e:
            message.fatal(e)

        # The scanned member values are more accurate than the values that the
        # we dumped from GEnumValue.value because GEnumValue.value has the
        # values as a 32-bit signed integer, even if they were unsigned
        # in the source code.
        previous_values = {}
        previous_symbols = {}
        previous = self._namespace.get(enum_name)
        if isinstance(previous, (ast.Enum, ast.Bitfield)):
            for member in previous.members:
                previous_values[member.name] = member.value
                previous_symbols[member.name] = member.symbol

        members = []
        for member in xmlnode.findall('member'):
            # Keep the name closer to what we'd take from C by default;
            # see http://bugzilla.gnome.org/show_bug.cgi?id=575613
            name = member.attrib['nick'].replace('-', '_')

            if name in previous_values:
                value = previous_values[name]
                symbol = previous_symbols[name]
            else:
                value = member.attrib['value']
                symbol = member.attrib['name']

            members.append(ast.Member(name,
                                      value,
                                      symbol,
                                      member.attrib['nick'],
                                      member.attrib['name']))

        if xmlnode.tag == 'flags':
            klass = ast.Bitfield
        else:
            klass = ast.Enum

        node = klass(enum_name, type_name,
                     gtype_name=type_name,
                     c_symbol_prefix=c_symbol_prefix,
                     members=members,
                     get_type=xmlnode.attrib['get-type'])
        self._namespace.append(node, replace=True)

    def _split_type_and_symbol_prefix(self, xmlnode):
        """Infer the C symbol prefix from the _get_type function."""
        get_type = xmlnode.attrib['get-type']
        (ns, name) = self._transformer.split_csymbol(get_type)
        assert ns is self._namespace
        if name in ('get_type', '_get_gtype'):
            message.fatal("""The GObject name '%s' isn't compatible
with the configured identifier prefixes:
  %r
The class would have no name.  Most likely you want to specify a
different --identifier-prefix.""" % (xmlnode.attrib['name'], self._namespace.identifier_prefixes))
        if name.endswith('_get_type'):
            type_suffix = '_get_type'
        else:
            type_suffix = '_get_gtype'
        return (get_type, name[:-len(type_suffix)])

    def _introspect_object(self, xmlnode):
        type_name = xmlnode.attrib['name']
        is_abstract = bool(xmlnode.attrib.get('abstract', False))
        is_final = bool(xmlnode.attrib.get('final', False))
        (get_type, c_symbol_prefix) = self._split_type_and_symbol_prefix(xmlnode)
        try:
            object_name = self._transformer.strip_identifier(type_name)
        except TransformerException as e:
            message.fatal(e)
        node = ast.Class(object_name, None,
                         gtype_name=type_name,
                         get_type=get_type,
                         c_symbol_prefix=c_symbol_prefix,
                         is_abstract=is_abstract,
                         is_final=is_final)
        self._parse_parents(xmlnode, node)
        self._introspect_properties(node, xmlnode)
        self._introspect_signals(node, xmlnode)
        self._introspect_implemented_interfaces(node, xmlnode)
        self._add_record_fields(node)
        self._namespace.append(node, replace=True)

    def _introspect_interface(self, xmlnode):
        type_name = xmlnode.attrib['name']
        (get_type, c_symbol_prefix) = self._split_type_and_symbol_prefix(xmlnode)
        try:
            interface_name = self._transformer.strip_identifier(type_name)
        except TransformerException as e:
            message.fatal(e)
        node = ast.Interface(interface_name, None,
                             gtype_name=type_name,
                             get_type=get_type,
                             c_symbol_prefix=c_symbol_prefix)
        self._introspect_properties(node, xmlnode)
        self._introspect_signals(node, xmlnode)
        for child in xmlnode.findall('prerequisite'):
            name = child.attrib['name']
            prereq = ast.Type.create_from_gtype_name(name)
            node.prerequisites.append(prereq)

        record = self._namespace.get(node.name)
        if isinstance(record, ast.Record):
            node.ctype = record.ctype
        else:
            message.warn_node(node, "Couldn't find associated structure for '%s'" % (node.name, ))

        # GtkFileChooserEmbed is an example of a private interface, we
        # just filter them out
        if xmlnode.attrib['get-type'].startswith('_'):
            self._private_internal_types[type_name] = node
        else:
            self._namespace.append(node, replace=True)

    # WORKAROUND
    # https://bugzilla.gnome.org/show_bug.cgi?id=550616
    def _introspect_boxed_gstreamer_workaround(self, xmlnode):
        node = ast.Boxed('ParamSpecMiniObject', gtype_name='GParamSpecMiniObject',
                         get_type='gst_param_spec_mini_object_get_type',
                         c_symbol_prefix='param_spec_mini_object')
        self._boxed_types[node.gtype_name] = node

    def _introspect_boxed(self, xmlnode):
        type_name = xmlnode.attrib['name']

        # Work around GStreamer legacy naming issue
        # https://bugzilla.gnome.org/show_bug.cgi?id=550616
        if type_name == 'GParamSpecMiniObject':
            self._introspect_boxed_gstreamer_workaround(xmlnode)
            return

        try:
            name = self._transformer.strip_identifier(type_name)
        except TransformerException as e:
            message.fatal(e)
        # This one doesn't go in the main namespace; we associate it with
        # the struct or union
        (get_type, c_symbol_prefix) = self._split_type_and_symbol_prefix(xmlnode)
        node = ast.Boxed(name, gtype_name=type_name,
                         get_type=get_type,
                         c_symbol_prefix=c_symbol_prefix)
        self._boxed_types[node.gtype_name] = node

    def _introspect_implemented_interfaces(self, node, xmlnode):
        gt_interfaces = []
        for interface in xmlnode.findall('implements'):
            gitype = ast.Type.create_from_gtype_name(interface.attrib['name'])
            gt_interfaces.append(gitype)
        node.interfaces = gt_interfaces

    def _introspect_properties(self, node, xmlnode):
        for pspec in xmlnode.findall('property'):
            ctype = pspec.attrib['type']
            flags = int(pspec.attrib['flags'])
            readable = (flags & G_PARAM_READABLE) != 0
            writable = (flags & G_PARAM_WRITABLE) != 0
            construct = (flags & G_PARAM_CONSTRUCT) != 0
            construct_only = (flags & G_PARAM_CONSTRUCT_ONLY) != 0
            default_value = pspec.attrib.get('default-value')
            prop = ast.Property(
                pspec.attrib['name'],
                ast.Type.create_from_gtype_name(ctype),
                readable, writable, construct, construct_only)
            prop.default_value = default_value
            node.properties.append(prop)
        node.properties = node.properties

    def _introspect_signals(self, node, xmlnode):
        for signal_info in xmlnode.findall('signal'):
            rctype = signal_info.attrib['return']
            rtype = ast.Type.create_from_gtype_name(rctype)
            return_ = ast.Return(rtype)
            parameters = []
            when = signal_info.attrib.get('when')
            no_recurse = signal_info.attrib.get('no-recurse', '0') == '1'
            detailed = signal_info.attrib.get('detailed', '0') == '1'
            action = signal_info.attrib.get('action', '0') == '1'
            no_hooks = signal_info.attrib.get('no-hooks', '0') == '1'
            for i, parameter in enumerate(signal_info.findall('param')):
                if i == 0:
                    argname = 'object'
                else:
                    argname = 'p%s' % (i - 1, )
                pctype = parameter.attrib['type']
                ptype = ast.Type.create_from_gtype_name(pctype)
                param = ast.Parameter(argname, ptype)
                param.transfer = ast.PARAM_TRANSFER_NONE
                parameters.append(param)
            signal = ast.Signal(signal_info.attrib['name'], return_, parameters,
                                when=when, no_recurse=no_recurse, detailed=detailed,
                                action=action, no_hooks=no_hooks)
            node.signals.append(signal)
        node.signals = node.signals

    def _parse_parents(self, xmlnode, node):
        parents_str = xmlnode.attrib.get('parents', '')
        if parents_str != '':
            parent_types = list(map(lambda s: ast.Type.create_from_gtype_name(s),
                                    parents_str.split(',')))
        else:
            parent_types = []
        node.parent_chain = parent_types

    def _introspect_fundamental(self, xmlnode):
        type_name = xmlnode.attrib['name']

        is_abstract = bool(xmlnode.attrib.get('abstract', False))
        is_final = bool(xmlnode.attrib.get('final', False))
        (get_type, c_symbol_prefix) = self._split_type_and_symbol_prefix(xmlnode)
        try:
            fundamental_name = self._transformer.strip_identifier(type_name)
        except TransformerException as e:
            message.warn(e)
            return

        node = ast.Class(fundamental_name, None,
                         gtype_name=type_name,
                         get_type=get_type,
                         c_symbol_prefix=c_symbol_prefix,
                         is_abstract=is_abstract,
                         is_final=is_final)
        self._parse_parents(xmlnode, node)
        node.fundamental = True
        self._introspect_implemented_interfaces(node, xmlnode)

        self._add_record_fields(node)
        self._namespace.append(node, replace=True)

    def _add_record_fields(self, node):
        # add record fields
        record = self._namespace.get(node.name)
        if not isinstance(record, ast.Record):
            return
        node.ctype = record.ctype
        node.fields = record.fields
        for field in node.fields:
            if isinstance(field, ast.Field):
                # Object instance fields are assumed to be read-only
                # (see also _find_class_record and transformer.py)
                field.writable = False

    def _introspect_error_quark(self, xmlnode):
        symbol = xmlnode.attrib['function']
        error_domain = xmlnode.attrib['domain']
        function = self._namespace.get_by_symbol(symbol)
        if function is None:
            return

        node = ast.ErrorQuarkFunction(function.name, function.retval,
                                      function.parameters, function.throws,
                                      function.symbol, error_domain)
        self._namespace.append(node, replace=True)

    def _pair_boxed_type(self, boxed):
        try:
            name = self._transformer.strip_identifier(boxed.gtype_name)
        except TransformerException as e:
            message.fatal(e)
        pair_node = self._namespace.get(name)
        if not pair_node:
            # Keep the "bare" boxed instance
            self._namespace.append(boxed)
        elif isinstance(pair_node, (ast.Record, ast.Union)):
            pair_node.add_gtype(boxed.gtype_name, boxed.get_type)
            assert boxed.c_symbol_prefix is not None
            pair_node.c_symbol_prefix = boxed.c_symbol_prefix
            # Backward compatibility hack - reset the disguised flag; we're
            # setting it incorrectly in the scanner. We don't change the
            # opaque flag, because it's a new one and we define its behavior
            pair_node.disguised = False
        else:
            return False

    def _find_class_record(self, cls):
        pair_record = None
        if isinstance(cls, ast.Class):
            pair_record = self._namespace.get(cls.name + 'Class')
        else:
            for suffix in ('Iface', 'Interface'):
                pair_record = self._namespace.get(cls.name + suffix)
                if pair_record:
                    break
        if not (pair_record and isinstance(pair_record, ast.Record)):
            return

        cls.glib_type_struct = pair_record.create_type()
        cls.inherit_file_positions(pair_record)
        pair_record.is_gtype_struct_for = cls.create_type()
