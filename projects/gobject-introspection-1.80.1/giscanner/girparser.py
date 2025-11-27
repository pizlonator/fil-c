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

from collections import OrderedDict
from xml.etree.ElementTree import parse

from . import ast
from .girwriter import COMPATIBLE_GIR_VERSION
from .message import Position

CORE_NS = "http://www.gtk.org/introspection/core/1.0"
C_NS = "http://www.gtk.org/introspection/c/1.0"
GLIB_NS = "http://www.gtk.org/introspection/glib/1.0"


def _corens(tag):
    return '{%s}%s' % (CORE_NS, tag)


def _glibns(tag):
    return '{%s}%s' % (GLIB_NS, tag)


def _cns(tag):
    return '{%s}%s' % (C_NS, tag)


class GIRParser(object):

    def __init__(self, types_only=False):
        self._types_only = types_only
        self._namespace = None
        self._filename_stack = []

    # Public API

    def parse(self, filename):
        filename = os.path.abspath(filename)
        self._filename_stack.append(filename)
        tree = parse(filename)
        self.parse_tree(tree)
        self._filename_stack.pop()

    def parse_tree(self, tree):
        self._namespace = None
        self._pkgconfig_packages = set()
        self._includes = set()
        self._c_includes = set()
        self._c_prefix = None
        self._parse_api(tree.getroot())

    def get_namespace(self):
        return self._namespace

    # Private

    def _find_first_child(self, node, name_or_names):
        if isinstance(name_or_names, str):
            for child in node:
                if child.tag == name_or_names:
                    return child
        else:
            for child in node:
                if child.tag in name_or_names:
                    return child
        return None

    def _find_children(self, node, name):
        return [child for child in node if child.tag == name]

    def _get_current_file(self):
        if not self._filename_stack:
            return None
        cwd = os.getcwd() + os.sep
        curfile = self._filename_stack[-1]
        if curfile.startswith(cwd):
            return curfile[len(cwd):]
        return curfile

    def _parse_api(self, root):
        assert root.tag == _corens('repository')
        version = root.attrib['version']
        if version != COMPATIBLE_GIR_VERSION:
            raise SystemExit("%s: Incompatible version %s (supported: %s)" %
                             (self._get_current_file(), version, COMPATIBLE_GIR_VERSION))

        for node in root:
            if node.tag == _corens('include'):
                self._parse_include(node)
            elif node.tag == _corens('package'):
                self._parse_pkgconfig_package(node)
            elif node.tag == _cns('include'):
                self._parse_c_include(node)

        ns = root.find(_corens('namespace'))
        assert ns is not None
        identifier_prefixes = ns.attrib.get(_cns('identifier-prefixes'))
        if identifier_prefixes:
            identifier_prefixes = identifier_prefixes.split(',')
        symbol_prefixes = ns.attrib.get(_cns('symbol-prefixes'))
        if symbol_prefixes:
            symbol_prefixes = symbol_prefixes.split(',')
        self._namespace = ast.Namespace(ns.attrib['name'],
                                        ns.attrib['version'],
                                        identifier_prefixes=identifier_prefixes,
                                        symbol_prefixes=symbol_prefixes)
        if 'shared-library' in ns.attrib:
            self._namespace.shared_libraries = ns.attrib['shared-library'].split(',')
        self._namespace.includes = self._includes
        self._namespace.c_includes = self._c_includes
        self._namespace.exported_packages = self._pkgconfig_packages

        parser_methods = {
            _corens('alias'): self._parse_alias,
            _corens('bitfield'): self._parse_enumeration_bitfield,
            _corens('callback'): self._parse_callback,
            _corens('class'): self._parse_object_interface,
            _corens('enumeration'): self._parse_enumeration_bitfield,
            _corens('interface'): self._parse_object_interface,
            _corens('record'): self._parse_record,
            _corens('union'): self._parse_union,
            _corens('docsection'): self._parse_doc_section,
            _glibns('boxed'): self._parse_boxed}

        if not self._types_only:
            parser_methods[_corens('constant')] = self._parse_constant
            parser_methods[_corens('function-inline')] = self._parse_function_inline
            parser_methods[_corens('function-macro')] = self._parse_function_macro
            parser_methods[_corens('function')] = self._parse_function

        for node in ns:
            method = parser_methods.get(node.tag)
            if method is not None:
                method(node)

    def _parse_doc_section(self, node):
        docsection = ast.DocSection(node.attrib["name"])
        self._parse_generic_attribs(node, docsection)
        self._namespace.append(docsection)

    def _parse_include(self, node):
        include = ast.Include(node.attrib['name'], node.attrib['version'])
        self._includes.add(include)

    def _parse_pkgconfig_package(self, node):
        self._pkgconfig_packages.add(node.attrib['name'])

    def _parse_c_include(self, node):
        self._c_includes.add(node.attrib['name'])

    def _parse_alias(self, node):
        typeval = self._parse_type(node)
        alias = ast.Alias(node.attrib['name'], typeval, node.attrib.get(_cns('type')))
        self._parse_generic_attribs(node, alias)
        self._namespace.append(alias)

    def _parse_generic_attribs(self, node, obj):
        assert isinstance(obj, ast.Annotated)
        skip = node.attrib.get('skip')
        if skip:
            try:
                obj.skip = int(skip) > 0
            except ValueError:
                obj.skip = False
        introspectable = node.attrib.get('introspectable')
        if introspectable:
            try:
                obj.introspectable = int(introspectable) > 0
            except ValueError:
                obj.introspectable = False
        if self._types_only:
            return
        doc = node.find(_corens('doc'))
        if doc is not None:
            if doc.text:
                obj.doc = doc.text
                obj.doc_position = Position(doc.attrib.get('filename', '<unknown>'),
                                            doc.attrib.get('line', None),
                                            doc.attrib.get('column', None))
        version = node.attrib.get('version')
        if version:
            obj.version = version
        version_doc = node.find(_corens('doc-version'))
        if version_doc is not None:
            if version_doc.text:
                obj.version_doc = version_doc.text
        deprecated = node.attrib.get('deprecated-version')
        if deprecated:
            obj.deprecated = deprecated
        deprecated_doc = node.find(_corens('doc-deprecated'))
        if deprecated_doc is not None:
            if deprecated_doc.text:
                obj.deprecated_doc = deprecated_doc.text
        stability = node.attrib.get('stability')
        if stability:
            obj.stability = stability
        stability_doc = node.find(_corens('doc-stability'))
        if stability_doc is not None:
            if stability_doc.text:
                obj.stability_doc = stability_doc.text
        attributes = node.findall(_corens('attribute'))
        if attributes:
            attributes_ = OrderedDict()
            for attribute in attributes:
                name = attribute.attrib.get('name')
                value = attribute.attrib.get('value')
                attributes_[name] = value
            obj.attributes = attributes_

        if hasattr(obj, 'add_file_position'):
            positions = sorted(node.findall(_corens('source-position')),
                               key=lambda x: (x.attrib['filename'],
                                              int(x.attrib['line'])))
            for position in positions:
                if 'column' in position.attrib:
                    column = int(position.attrib['column'])
                else:
                    column = None

                obj.add_file_position(Position(position.attrib['filename'],
                                               int(position.attrib['line']),
                                               column))

    def _parse_object_interface(self, node):
        parent = node.attrib.get('parent')
        if parent:
            parent_type = self._namespace.type_from_name(parent)
        else:
            parent_type = None

        ctor_kwargs = {'name': node.attrib['name'],
                       'parent_type': parent_type,
                       'gtype_name': node.attrib[_glibns('type-name')],
                       'get_type': node.attrib[_glibns('get-type')],
                       'c_symbol_prefix': node.attrib.get(_cns('symbol-prefix')),
                       'ctype': node.attrib.get(_cns('type'))}
        if node.tag == _corens('interface'):
            klass = ast.Interface
        elif node.tag == _corens('class'):
            klass = ast.Class
            is_abstract = node.attrib.get('abstract')
            is_abstract = is_abstract and is_abstract != '0'
            ctor_kwargs['is_abstract'] = is_abstract
            is_final = node.attrib.get('final')
            is_final = is_final and is_final != '0'
            ctor_kwargs['is_final'] = is_final
        else:
            raise AssertionError(node)

        obj = klass(**ctor_kwargs)
        self._parse_generic_attribs(node, obj)
        type_struct = node.attrib.get(_glibns('type-struct'))
        if type_struct:
            obj.glib_type_struct = self._namespace.type_from_name(type_struct)
        if klass == ast.Class:
            is_fundamental = node.attrib.get(_glibns('fundamental'))
            if is_fundamental and is_fundamental != '0':
                obj.fundamental = True
            for func_id in ['ref-func', 'unref-func',
                            'set-value-func', 'get-value-func']:
                func_name = node.attrib.get(_glibns(func_id))
                obj.__dict__[func_id.replace('-', '_')] = func_name

        if self._types_only:
            self._namespace.append(obj)
            return

        for iface in self._find_children(node, _corens('implements')):
            obj.interfaces.append(self._namespace.type_from_name(iface.attrib['name']))
        for iface in self._find_children(node, _corens('prerequisite')):
            obj.prerequisites.append(self._namespace.type_from_name(iface.attrib['name']))
        for func_node in self._find_children(node, _corens('function')):
            func = self._parse_function_common(func_node, ast.Function, obj)
            obj.static_methods.append(func)
        for method in self._find_children(node, _corens('method')):
            func = self._parse_function_common(method, ast.Function, obj)
            func.is_method = True
            obj.methods.append(func)
        for method in self._find_children(node, _corens('method-inline')):
            func = self._parse_function_common(method, ast.Function, obj)
            func.is_method = True
            func.is_inline = True
            obj.methods.append(func)
        for method in self._find_children(node, _corens('virtual-method')):
            func = self._parse_function_common(method, ast.VFunction, obj)
            self._parse_generic_attribs(method, func)
            func.is_method = True
            func.invoker = method.get('invoker')
            obj.virtual_methods.append(func)
        for ctor in self._find_children(node, _corens('constructor')):
            func = self._parse_function_common(ctor, ast.Function, obj)
            func.is_constructor = True
            obj.constructors.append(func)
        obj.fields.extend(self._parse_fields(node, obj))
        for prop in self._find_children(node, _corens('property')):
            obj.properties.append(self._parse_property(prop, obj))
        for signal in self._find_children(node, _glibns('signal')):
            obj.signals.append(self._parse_function_common(signal, ast.Signal, obj))

        self._namespace.append(obj)

    def _parse_callback(self, node):
        callback = self._parse_function_common(node, ast.Callback)
        self._namespace.append(callback)

    def _parse_function(self, node):
        function = self._parse_function_common(node, ast.Function)
        self._namespace.append(function)

    def _parse_function_inline(self, node):
        function = self._parse_function_common(node, ast.Function)
        function.is_inline = True
        self._namespace.append(function)

    def _parse_function_macro(self, node):
        name = node.attrib['name']
        symbol = node.attrib.get(_cns('identifier'))
        parameters = []
        parameters_node = node.find(_corens('parameters'))
        if (parameters_node is not None):
            for paramnode in self._find_children(parameters_node, _corens('parameter')):
                param = ast.Parameter(paramnode.attrib.get('name'), None)
                parameters.append(param)
                self._parse_generic_attribs(paramnode, param)

        func = ast.FunctionMacro(name, parameters, symbol)
        self._parse_generic_attribs(node, func)

        self._namespace.track(func)
        self._namespace.append(func)

    def _parse_parameter(self, node):
        typeval = self._parse_type(node)
        param = ast.Parameter(node.attrib.get('name'),
                              typeval,
                              node.attrib.get('direction') or ast.PARAM_DIRECTION_IN,
                              node.attrib.get('transfer-ownership'),
                              node.attrib.get('nullable') == '1',
                              node.attrib.get('optional') == '1',
                              node.attrib.get('allow-none') == '1',
                              node.attrib.get('scope'),
                              node.attrib.get('caller-allocates') == '1')
        self._parse_generic_attribs(node, param)
        return param

    def _parse_function_common(self, node, klass, parent=None):
        name = node.attrib['name']
        returnnode = node.find(_corens('return-value'))
        if not returnnode:
            raise ValueError('node %r has no return-value' % (name, ))
        transfer = returnnode.attrib.get('transfer-ownership')
        nullable = returnnode.attrib.get('nullable') == '1'
        retval = ast.Return(self._parse_type(returnnode), nullable, False, transfer)
        self._parse_generic_attribs(returnnode, retval)
        parameters = []

        throws = (node.attrib.get('throws') == '1')

        if klass is ast.Callback:
            func = klass(name, retval, parameters, throws,
                         node.attrib.get(_cns('type')))
        elif klass is ast.Function:
            identifier = node.attrib.get(_cns('identifier'))
            func = klass(name, retval, parameters, throws, identifier)
        elif klass is ast.VFunction:
            func = klass(name, retval, parameters, throws)
        elif klass is ast.Signal:
            func = klass(name, retval, parameters,
                         when=node.attrib.get('when'),
                         no_recurse=node.attrib.get('no-recurse', '0') == '1',
                         detailed=node.attrib.get('detailed', '0') == '1',
                         action=node.attrib.get('action', '0') == '1',
                         no_hooks=node.attrib.get('no-hooks', '0') == '1')
            func.emitter = node.attrib.get('emitter')
        else:
            assert False

        func.set_property = node.attrib.get(_glibns('set-property'), None)
        func.get_property = node.attrib.get(_glibns('get-property'), None)
        func.shadows = node.attrib.get('shadows', None)
        func.shadowed_by = node.attrib.get('shadowed-by', None)
        func.moved_to = node.attrib.get('moved-to', None)
        func.finish_func = node.attrib.get(_glibns('finish-func'), None)
        func.sync_func = node.attrib.get(_glibns('sync-func'), None)
        func.async_func = node.attrib.get(_glibns('async-func'), None)
        func.parent = parent

        parameters_node = node.find(_corens('parameters'))
        if (parameters_node is not None):
            paramnode = self._find_first_child(parameters_node, _corens('instance-parameter'))
            if paramnode:
                func.instance_parameter = self._parse_parameter(paramnode)
            for paramnode in self._find_children(parameters_node, _corens('parameter')):
                parameters.append(self._parse_parameter(paramnode))
            for i, paramnode in enumerate(self._find_children(parameters_node,
                                                              _corens('parameter'))):
                param = parameters[i]
                self._parse_type_array_length(parameters, paramnode, param.type)
                closure = paramnode.attrib.get('closure')
                if closure:
                    idx = int(closure)
                    assert idx < len(parameters), "%d >= %d" % (idx, len(parameters))
                    param.closure_name = parameters[idx].argname
                destroy = paramnode.attrib.get('destroy')
                if destroy:
                    idx = int(destroy)
                    assert idx < len(parameters), "%d >= %d" % (idx, len(parameters))
                    param.destroy_name = parameters[idx].argname

        self._parse_type_array_length(parameters, returnnode, retval.type)

        # Re-set the function's parameters to notify it of changes to the list.
        func.parameters = parameters

        self._parse_generic_attribs(node, func)

        self._namespace.track(func)
        return func

    def _parse_fields(self, node, obj):
        res = []
        names = (_corens('field'), _corens('record'), _corens('union'), _corens('callback'))
        for child in node:
            if child.tag in names:
                fieldobj = self._parse_field(child, obj)
                res.append(fieldobj)
        return res

    def _parse_compound(self, cls, node):
        compound = cls(node.attrib.get('name'),
                       ctype=node.attrib.get(_cns('type')),
                       disguised=node.attrib.get('disguised') == '1',
                       opaque=node.attrib.get('opaque') == '1',
                       pointer=node.attrib.get('pointer') == '1',
                       gtype_name=node.attrib.get(_glibns('type-name')),
                       get_type=node.attrib.get(_glibns('get-type')),
                       c_symbol_prefix=node.attrib.get(_cns('symbol-prefix')))
        if node.attrib.get('foreign') == '1':
            compound.foreign = True
        self._parse_generic_attribs(node, compound)
        if not self._types_only:
            compound.fields.extend(self._parse_fields(node, compound))
            for method in self._find_children(node, _corens('method')):
                func = self._parse_function_common(method, ast.Function, compound)
                func.is_method = True
                compound.methods.append(func)
            for method in self._find_children(node, _corens('method-inline')):
                func = self._parse_function_common(method, ast.Function, compound)
                func.is_method = True
                func.is_inline = True
                compound.methods.append(func)
            for i, fieldnode in enumerate(self._find_children(node, _corens('field'))):
                field = compound.fields[i]
                self._parse_type_array_length(compound.fields, fieldnode, field.type)
            for func in self._find_children(node, _corens('function')):
                compound.static_methods.append(
                    self._parse_function_common(func, ast.Function, compound))
            for ctor in self._find_children(node, _corens('constructor')):
                func = self._parse_function_common(ctor, ast.Function, compound)
                func.is_constructor = True
                compound.constructors.append(func)
        return compound

    def _parse_record(self, node, anonymous=False):
        struct = self._parse_compound(ast.Record, node)
        is_gtype_struct_for = node.attrib.get(_glibns('is-gtype-struct-for'))
        copy_func = node.attrib.get('copy-function')
        free_func = node.attrib.get('free-function')
        if is_gtype_struct_for is not None:
            struct.is_gtype_struct_for = self._namespace.type_from_name(is_gtype_struct_for)
        struct.copy_func = copy_func
        struct.free_func = free_func
        if not anonymous:
            self._namespace.append(struct)
        return struct

    def _parse_union(self, node, anonymous=False):
        union = self._parse_compound(ast.Union, node)
        union.copy_func = node.attrib.get('copy-function')
        union.free_func = node.attrib.get('free-function')
        if not anonymous:
            self._namespace.append(union)
        return union

    def _parse_type_simple(self, typenode):
        # ast.Fields can contain inline callbacks
        if typenode.tag == _corens('callback'):
            typeval = self._namespace.type_from_name(typenode.attrib['name'])
            typeval.ctype = typenode.attrib.get(_cns('type'))
            return typeval
        # ast.Arrays have their own toplevel XML
        elif typenode.tag == _corens('array'):
            array_type = typenode.attrib.get('name')
            element_type = self._parse_type(typenode)
            array_ctype = typenode.attrib.get(_cns('type'))
            ret = ast.Array(array_type, element_type, ctype=array_ctype)
            # zero-terminated defaults to true...
            zero = typenode.attrib.get('zero-terminated')
            if zero and zero == '0':
                ret.zeroterminated = False
            fixed_size = typenode.attrib.get('fixed-size')
            if fixed_size:
                ret.size = int(fixed_size)

            return ret
        elif typenode.tag == _corens('varargs'):
            return ast.Varargs()
        elif typenode.tag == _corens('type'):
            name = typenode.attrib.get('name')
            ctype = typenode.attrib.get(_cns('type'))
            if name is None:
                if ctype is None:
                    return ast.TypeUnknown()
                return ast.Type(ctype=ctype)
            elif name in ['GLib.List', 'GLib.SList']:
                subchild = self._find_first_child(typenode,
                                                  list(map(_corens, ('callback', 'array',
                                                                '    varargs', 'type'))))
                if subchild is not None:
                    element_type = self._parse_type(typenode)
                else:
                    element_type = ast.TYPE_ANY
                return ast.List(name, element_type, ctype=ctype)
            elif name == 'GLib.HashTable':
                subchildren = self._find_children(typenode, _corens('type'))
                subchildren_types = list(map(self._parse_type_simple, subchildren))
                while len(subchildren_types) < 2:
                    subchildren_types.append(ast.TYPE_ANY)
                return ast.Map(subchildren_types[0], subchildren_types[1], ctype=ctype)
            else:
                return self._namespace.type_from_name(name, ctype)
        else:
            assert False, "Failed to parse inner type"

    def _parse_type(self, node):
        for name in map(_corens, ('callback', 'array', 'varargs', 'type')):
            typenode = node.find(name)
            if typenode is not None:
                return self._parse_type_simple(typenode)
        assert False, "Failed to parse toplevel type"

    def _parse_type_array_length(self, siblings, node, typeval):
        """A hack necessary to handle the integer parameter/field indexes on
           array types."""
        typenode = node.find(_corens('array'))
        if typenode is None:
            return
        lenidx = typenode.attrib.get('length')
        if lenidx is not None:
            idx = int(lenidx)
            assert idx < len(siblings), "%r %d >= %d" % (siblings, idx, len(siblings))
            if isinstance(siblings[idx], ast.Field):
                typeval.length_param_name = siblings[idx].name
            else:
                typeval.length_param_name = siblings[idx].argname

    def _parse_boxed(self, node):
        obj = ast.Boxed(node.attrib[_glibns('name')],
                        gtype_name=node.attrib[_glibns('type-name')],
                        get_type=node.attrib[_glibns('get-type')],
                        c_symbol_prefix=node.attrib.get(_cns('symbol-prefix')))
        self._parse_generic_attribs(node, obj)

        if self._types_only:
            self._namespace.append(obj)
            return

        for method in self._find_children(node, _corens('method')):
            func = self._parse_function_common(method, ast.Function, obj)
            func.is_method = True
            obj.methods.append(func)
        for method in self._find_children(node, _corens('method-inline')):
            func = self._parse_function_common(method, ast.Function, obj)
            func.is_inline = True
            func.is_method = True
            obj.methods.append(func)
        for ctor in self._find_children(node, _corens('constructor')):
            obj.constructors.append(
                self._parse_function_common(ctor, ast.Function, obj))
        for callback in self._find_children(node, _corens('callback')):
            obj.fields.append(
                self._parse_function_common(callback, ast.Callback, obj))
        self._namespace.append(obj)

    def _parse_field(self, node, parent):
        type_node = None
        anonymous_node = None
        if node.tag in map(_corens, ('callback', 'record', 'union')):
            anonymous_elt = node
        else:
            anonymous_elt = self._find_first_child(node, _corens('callback'))
        if anonymous_elt is not None:
            if anonymous_elt.tag == _corens('callback'):
                anonymous_node = self._parse_function_common(anonymous_elt, ast.Callback)
            elif anonymous_elt.tag == _corens('record'):
                anonymous_node = self._parse_record(anonymous_elt, anonymous=True)
            elif anonymous_elt.tag == _corens('union'):
                anonymous_node = self._parse_union(anonymous_elt, anonymous=True)
            else:
                assert False, anonymous_elt.tag
        else:
            assert node.tag == _corens('field'), node.tag
            type_node = self._parse_type(node)
        field = ast.Field(node.attrib.get('name'),
                          type_node,
                          node.attrib.get('readable') != '0',
                          node.attrib.get('writable') == '1',
                          node.attrib.get('bits'),
                          anonymous_node=anonymous_node)
        field.private = node.attrib.get('private') == '1'
        field.parent = parent
        self._parse_generic_attribs(node, field)
        return field

    def _parse_property(self, node, parent):
        prop = ast.Property(node.attrib['name'],
                            self._parse_type(node),
                            node.attrib.get('readable') != '0',
                            node.attrib.get('writable') == '1',
                            node.attrib.get('construct') == '1',
                            node.attrib.get('construct-only') == '1',
                            node.attrib.get('transfer-ownership'))
        prop.setter = node.attrib.get('setter')
        prop.getter = node.attrib.get('getter')
        prop.default_value = node.attrib.get('default-value')
        prop.parent = parent
        self._parse_generic_attribs(node, prop)
        return prop

    def _parse_member(self, node):
        member = ast.Member(node.attrib['name'],
                            node.attrib['value'],
                            node.attrib.get(_cns('identifier')),
                            node.attrib.get(_glibns('nick')),
                            node.attrib.get(_glibns('name')))
        self._parse_generic_attribs(node, member)
        return member

    def _parse_constant(self, node):
        type_node = self._parse_type(node)
        constant = ast.Constant(node.attrib['name'],
                                type_node,
                                node.attrib['value'],
                                node.attrib.get(_cns('type')))
        self._parse_generic_attribs(node, constant)
        self._namespace.append(constant)

    def _parse_enumeration_bitfield(self, node):
        name = node.attrib.get('name')
        ctype = node.attrib.get(_cns('type'))
        get_type = node.attrib.get(_glibns('get-type'))
        type_name = node.attrib.get(_glibns('type-name'))
        glib_error_domain = node.attrib.get(_glibns('error-domain'))
        if node.tag == _corens('bitfield'):
            klass = ast.Bitfield
        else:
            klass = ast.Enum
        members = []
        obj = klass(name, ctype,
                    members=members,
                    gtype_name=type_name,
                    get_type=get_type)
        obj.error_domain = glib_error_domain
        obj.ctype = ctype
        self._parse_generic_attribs(node, obj)

        if self._types_only:
            self._namespace.append(obj)
            return

        for member_node in self._find_children(node, _corens('member')):
            member = self._parse_member(member_node)
            member.parent = obj
            members.append(member)
        for func_node in self._find_children(node, _corens('function')):
            func = self._parse_function_common(func_node, ast.Function)
            func.parent = obj
            obj.static_methods.append(func)
        self._namespace.append(obj)
