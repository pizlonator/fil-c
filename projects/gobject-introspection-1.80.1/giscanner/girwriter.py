# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008  Johan Dahlin
# Copyright (C) 2008, 2009 Red Hat, Inc.
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

from . import ast
from .xmlwriter import XMLWriter

# Bump this for *incompatible* changes to the .gir.
# Compatible changes we just make inline
COMPATIBLE_GIR_VERSION = '1.2'


class GIRWriter(XMLWriter):

    def __init__(self, namespace, sources_roots=[]):
        super(GIRWriter, self).__init__()
        self.write_comment(
            'This file was automatically generated from C sources - DO NOT EDIT!\n'
            'To affect the contents of this file, edit the original C definitions,\n'
            'and/or use gtk-doc annotations. ')
        self.sources_roots = sources_roots
        self._write_repository(namespace)

    def _write_repository(self, namespace):
        attrs = [
            ('version', COMPATIBLE_GIR_VERSION),
            ('xmlns', 'http://www.gtk.org/introspection/core/1.0'),
            ('xmlns:c', 'http://www.gtk.org/introspection/c/1.0'),
            ('xmlns:glib', 'http://www.gtk.org/introspection/glib/1.0')]
        with self.tagcontext('repository', attrs):
            for include in sorted(namespace.includes):
                self._write_include(include)
            for pkg in sorted(set(namespace.exported_packages)):
                self._write_pkgconfig_pkg(pkg)
            for c_include in sorted(set(namespace.c_includes)):
                self._write_c_include(c_include)
            self._namespace = namespace
            self._write_namespace(namespace)
            self._namespace = None

    def _write_include(self, include):
        attrs = [('name', include.name), ('version', include.version)]
        self.write_tag('include', attrs)

    def _write_pkgconfig_pkg(self, package):
        attrs = [('name', package)]
        self.write_tag('package', attrs)

    def _write_c_include(self, c_include):
        attrs = [('name', c_include)]
        self.write_tag('c:include', attrs)

    def _write_namespace(self, namespace):
        attrs = [('name', namespace.name),
                 ('version', namespace.version),
                 ('shared-library', ','.join(namespace.shared_libraries)),
                 ('c:identifier-prefixes', ','.join(namespace.identifier_prefixes)),
                 ('c:symbol-prefixes', ','.join(namespace.symbol_prefixes))]
        with self.tagcontext('namespace', attrs):
            # We define a custom sorting function here because
            # we want aliases to be first.  They're a bit
            # special because the typelib compiler expands them.
            def nscmp(val):
                if isinstance(val, ast.Alias):
                    return 0, val
                else:
                    return 1, val
            for node in sorted(namespace.values(), key=nscmp):
                self._write_node(node)

    def _write_node(self, node):
        if isinstance(node, ast.Function):
            self._write_function(node)
        elif isinstance(node, ast.FunctionMacro):
            self._write_function_macro(node)
        elif isinstance(node, ast.Enum):
            self._write_enum(node)
        elif isinstance(node, ast.Bitfield):
            self._write_bitfield(node)
        elif isinstance(node, (ast.Class, ast.Interface)):
            self._write_class(node)
        elif isinstance(node, ast.Callback):
            self._write_callback(node)
        elif isinstance(node, ast.Record):
            self._write_record(node)
        elif isinstance(node, ast.Union):
            self._write_union(node)
        elif isinstance(node, ast.Boxed):
            self._write_boxed(node)
        elif isinstance(node, ast.Member):
            # FIXME: atk_misc_instance singleton
            pass
        elif isinstance(node, ast.Alias):
            self._write_alias(node)
        elif isinstance(node, ast.Constant):
            self._write_constant(node)
        elif isinstance(node, ast.DocSection):
            self._write_doc_section(node)
        else:
            print('WRITER: Unhandled node', node)

    def _append_version(self, node, attrs):
        if node.version:
            attrs.append(('version', node.version))

    def _get_relative_path(self, filename):
        res = filename
        for root in self.sources_roots:
            relpath = ''
            try:
                relpath = os.path.relpath(filename, root)

            # We might be on different drives on Windows, so relpath() won't work
            except ValueError:
                relpath = filename

            if len(relpath) < len(res):
                res = relpath

        return res

    def _write_generic(self, node):
        for key, value in node.attributes.items():
            self.write_tag('attribute', [('name', key), ('value', value)])

        if hasattr(node, 'doc') and node.doc:
            attrs = [('xml:space', 'preserve'),
                    ('filename', self._get_relative_path(node.doc_position.filename)),
                    ('line', str(node.doc_position.line))]
            if node.doc_position.column:
                attrs.append(('column', str(node.doc_position.column)))

            self.write_tag('doc', attrs, node.doc)

        if hasattr(node, 'version_doc') and node.version_doc:
            self.write_tag('doc-version', [('xml:space', 'preserve')],
                           node.version_doc)

        if hasattr(node, 'deprecated_doc') and node.deprecated_doc:
            self.write_tag('doc-deprecated', [('xml:space', 'preserve')],
                           node.deprecated_doc)

        if hasattr(node, 'stability_doc') and node.stability_doc:
            self.write_tag('doc-stability', [('xml:space', 'preserve')],
                           node.stability_doc)

        filepos = getattr(node, 'get_main_position', lambda: None)()
        if filepos is not None:
            position = [('filename', self._get_relative_path(filepos.filename)),
                        ('line', str(filepos.line))]
            if filepos.column:
                position.append(('column', str(filepos.column)))
            self.write_tag('source-position', position)

    def _append_node_generic(self, node, attrs):
        if node.skip or not node.introspectable:
            attrs.append(('introspectable', '0'))

        if node.deprecated or node.deprecated_doc:
            # The deprecated attribute used to contain node.deprecated_doc as an attribute. As
            # an xml attribute cannot preserve whitespace, deprecated_doc has been moved into
            # it's own tag, written in _write_generic() above. We continue to write the deprecated
            # attribute for backwards compatibility
            attrs.append(('deprecated', '1'))

        if node.deprecated:
            attrs.append(('deprecated-version', node.deprecated))

        if node.stability:
            attrs.append(('stability', node.stability))

    def _append_throws(self, func, attrs):
        if func.throws:
            attrs.append(('throws', '1'))

    def _write_alias(self, alias):
        attrs = [('name', alias.name)]
        if alias.ctype is not None:
            attrs.append(('c:type', alias.ctype))
        self._append_node_generic(alias, attrs)
        with self.tagcontext('alias', attrs):
            self._write_generic(alias)
            self._write_type_ref(alias.target)

    def _write_callable(self, callable, tag_name, extra_attrs):
        attrs = [('name', callable.name)]
        attrs.extend(extra_attrs)
        self._append_version(callable, attrs)
        self._append_node_generic(callable, attrs)
        self._append_throws(callable, attrs)
        if callable.finish_func is not None:
            attrs.append(('glib:finish-func', callable.finish_func))
        if callable.sync_func is not None:
            attrs.append(('glib:sync-func', callable.sync_func))
        if callable.async_func is not None:
            attrs.append(('glib:async-func', callable.async_func))
        with self.tagcontext(tag_name, attrs):
            self._write_generic(callable)
            self._write_return_type(callable.retval, parent=callable)
            self._write_parameters(callable)

    def _write_function_common(self, func, tag_name='function'):
        if func.internal_skipped:
            return
        attrs = []
        if hasattr(func, 'symbol'):
            attrs.append(('c:identifier', func.symbol))
        if func.shadowed_by:
            attrs.append(('shadowed-by', func.shadowed_by))
        elif func.shadows:
            attrs.append(('shadows', func.shadows))
        if func.moved_to is not None:
            attrs.append(('moved-to', func.moved_to))
        if func.set_property is not None:
            attrs.append(('glib:set-property', func.set_property))
        if func.get_property is not None:
            attrs.append(('glib:get-property', func.get_property))
        self._write_callable(func, tag_name, attrs)

    def _write_function_macro(self, macro):
        attrs = [('name', macro.name),
                 ('c:identifier', macro.symbol)]
        self._append_version(macro, attrs)
        self._append_node_generic(macro, attrs)
        with self.tagcontext('function-macro', attrs):
            self._write_generic(macro)
            self._write_untyped_parameters(macro)

    def _write_function(self, function):
        if function.is_inline:
            self._write_function_common(function, tag_name='function-inline')
        else:
            self._write_function_common(function, tag_name='function')

    def _write_method(self, method):
        if method.is_inline:
            self._write_function_common(method, tag_name='method-inline')
        else:
            self._write_function_common(method, tag_name='method')

    def _write_static_method(self, method):
        self._write_function_common(method, tag_name='function')

    def _write_constructor(self, method):
        self._write_function_common(method, tag_name='constructor')

    def _write_return_type(self, return_, parent=None):
        if not return_:
            return

        attrs = []
        if return_.transfer:
            attrs.append(('transfer-ownership', return_.transfer))
        if return_.skip:
            attrs.append(('skip', '1'))
        if return_.nullable and not return_.not_nullable:
            attrs.append(('nullable', '1'))
        with self.tagcontext('return-value', attrs):
            self._write_generic(return_)
            self._write_type(return_.type, parent=parent)

    def _write_parameters(self, callable):
        if not callable.parameters and callable.instance_parameter is None:
            return
        with self.tagcontext('parameters'):
            if callable.instance_parameter:
                self._write_parameter(callable, callable.instance_parameter, 'instance-parameter')
            for parameter in callable.parameters:
                self._write_parameter(callable, parameter)

    def _write_untyped_parameters(self, macro):
        if not macro.parameters:
            return
        with self.tagcontext('parameters'):
            for parameter in macro.parameters:
                self._write_untyped_parameter(macro, parameter)

    def _write_untyped_parameter(self, macro, parameter):
        attrs = []
        if parameter.argname is not None:
            attrs.append(('name', parameter.argname))
        with self.tagcontext('parameter', attrs):
            self._write_generic(parameter)

    def _write_parameter(self, parent, parameter, nodename='parameter'):
        attrs = []
        if parameter.argname is not None:
            attrs.append(('name', parameter.argname))
        if (parameter.direction is not None) and (parameter.direction != 'in'):
            attrs.append(('direction', parameter.direction))
            attrs.append(('caller-allocates',
                          '1' if parameter.caller_allocates else '0'))
        if parameter.transfer:
            attrs.append(('transfer-ownership',
                          parameter.transfer))
        if parameter.nullable and not parameter.not_nullable:
            attrs.append(('nullable', '1'))
            if parameter.direction != ast.PARAM_DIRECTION_OUT:
                attrs.append(('allow-none', '1'))
        if parameter.optional:
            attrs.append(('optional', '1'))
            if parameter.direction == ast.PARAM_DIRECTION_OUT:
                attrs.append(('allow-none', '1'))
        if parameter.scope:
            attrs.append(('scope', parameter.scope))
        if parameter.closure_name is not None:
            idx = parent.get_parameter_index(parameter.closure_name)
            attrs.append(('closure', '%d' % (idx, )))
        if parameter.destroy_name is not None:
            idx = parent.get_parameter_index(parameter.destroy_name)
            attrs.append(('destroy', '%d' % (idx, )))
        if parameter.skip:
            attrs.append(('skip', '1'))
        with self.tagcontext(nodename, attrs):
            self._write_generic(parameter)
            self._write_type(parameter.type, parent=parent)

    def _type_to_name(self, typeval):
        if not typeval.resolved:
            raise AssertionError("Caught unresolved type %r (ctype=%r)" % (typeval, typeval.ctype))
        assert typeval.target_giname is not None
        prefix = self._namespace.name + '.'
        if typeval.target_giname.startswith(prefix):
            return typeval.target_giname[len(prefix):]
        return typeval.target_giname

    def _write_type_ref(self, ntype):
        """ Like _write_type, but only writes the type name rather than the full details """
        assert isinstance(ntype, ast.Type), ntype
        attrs = []
        if ntype.ctype:
            attrs.append(('c:type', ntype.complete_ctype or ntype.ctype))
        if isinstance(ntype, ast.Array):
            if ntype.array_type != ast.Array.C:
                attrs.insert(0, ('name', ntype.array_type))
        elif isinstance(ntype, ast.List):
            if ntype.name:
                attrs.insert(0, ('name', ntype.name))
        elif isinstance(ntype, ast.Map):
            attrs.insert(0, ('name', 'GLib.HashTable'))
        else:
            if ntype.target_giname:
                attrs.insert(0, ('name', self._type_to_name(ntype)))
            elif ntype.target_fundamental:
                attrs.insert(0, ('name', ntype.target_fundamental))

        self.write_tag('type', attrs)

    def _write_type(self, ntype, relation=None, parent=None):
        assert isinstance(ntype, ast.Type), ntype
        attrs = []
        if ntype.complete_ctype:
            attrs.append(('c:type', ntype.complete_ctype))
        elif ntype.ctype:
            attrs.append(('c:type', ntype.ctype))
        if isinstance(ntype, ast.Varargs):
            self.write_tag('varargs', [])
        elif isinstance(ntype, ast.Array):
            if ntype.array_type != ast.Array.C:
                attrs.insert(0, ('name', ntype.array_type))
            # we insert an explicit 'zero-terminated' attribute
            # when it is false, or when it would not be implied
            # by the absence of length and fixed-size
            if not ntype.zeroterminated:
                attrs.insert(0, ('zero-terminated', '0'))
            elif (ntype.zeroterminated
                  and (ntype.size is not None or ntype.length_param_name is not None)):
                attrs.insert(0, ('zero-terminated', '1'))
            if ntype.size is not None:
                attrs.append(('fixed-size', '%d' % (ntype.size, )))
            if ntype.length_param_name is not None:
                if isinstance(parent, ast.Callable):
                    length = parent.get_parameter_index(ntype.length_param_name)
                elif isinstance(parent, ast.Compound):
                    length = parent.get_field_index(ntype.length_param_name)
                else:
                    assert False, "parent not a callable or compound: %r" % parent
                attrs.insert(0, ('length', '%d' % (length, )))

            with self.tagcontext('array', attrs):
                self._write_type(ntype.element_type)
        elif isinstance(ntype, ast.List):
            if ntype.name:
                attrs.insert(0, ('name', ntype.name))
            with self.tagcontext('type', attrs):
                self._write_type(ntype.element_type)
        elif isinstance(ntype, ast.Map):
            attrs.insert(0, ('name', 'GLib.HashTable'))
            with self.tagcontext('type', attrs):
                self._write_type(ntype.key_type)
                self._write_type(ntype.value_type)
        else:
            # REWRITEFIXME - enable this for 1.2
            if ntype.target_giname:
                attrs.insert(0, ('name', self._type_to_name(ntype)))
            elif ntype.target_fundamental:
                # attrs = [('fundamental', ntype.target_fundamental)]
                attrs.insert(0, ('name', ntype.target_fundamental))
            elif ntype.target_foreign:
                attrs.insert(0, ('foreign', '1'))
            self.write_tag('type', attrs)

    def _append_registered(self, node, attrs):
        assert isinstance(node, ast.Registered)
        if node.get_type:
            attrs.extend([('glib:type-name', node.gtype_name),
                          ('glib:get-type', node.get_type)])

    def _write_enum(self, enum):
        attrs = [('name', enum.name)]
        self._append_version(enum, attrs)
        self._append_node_generic(enum, attrs)
        self._append_registered(enum, attrs)
        attrs.append(('c:type', enum.ctype))
        if enum.error_domain:
            attrs.append(('glib:error-domain', enum.error_domain))

        with self.tagcontext('enumeration', attrs):
            self._write_generic(enum)
            for member in enum.members:
                self._write_member(member)
            for method in sorted(enum.static_methods):
                self._write_static_method(method)

    def _write_bitfield(self, bitfield):
        attrs = [('name', bitfield.name)]
        self._append_version(bitfield, attrs)
        self._append_node_generic(bitfield, attrs)
        self._append_registered(bitfield, attrs)
        attrs.append(('c:type', bitfield.ctype))
        with self.tagcontext('bitfield', attrs):
            self._write_generic(bitfield)
            for member in bitfield.members:
                self._write_member(member)
            for method in sorted(bitfield.static_methods):
                self._write_static_method(method)

    def _write_member(self, member):
        attrs = [('name', member.name),
                 ('value', str(member.value)),
                 ('c:identifier', member.symbol)]
        self._append_version(member, attrs)
        if member.nick is not None:
            attrs.append(('glib:nick', member.nick))
        if member.dump_name is not None:
            attrs.append(('glib:name', member.dump_name))
        with self.tagcontext('member', attrs):
            self._write_generic(member)

    def _write_doc_section(self, doc_section):
        attrs = [('name', doc_section.name)]
        with self.tagcontext('docsection', attrs):
            self._write_generic(doc_section)

    def _write_constant(self, constant):
        attrs = [('name', constant.name),
                 ('value', constant.value),
                 ('c:type', constant.ctype)]
        self._append_version(constant, attrs)
        self._append_node_generic(constant, attrs)
        with self.tagcontext('constant', attrs):
            self._write_generic(constant)
            self._write_type(constant.value_type)

    def _write_class(self, node):
        attrs = [('name', node.name),
                 ('c:symbol-prefix', node.c_symbol_prefix),
                 ('c:type', node.ctype)]
        self._append_version(node, attrs)
        self._append_node_generic(node, attrs)
        if isinstance(node, ast.Class):
            tag_name = 'class'
            if node.parent_type is not None:
                attrs.append(('parent',
                              self._type_to_name(node.parent_type)))
            if node.is_abstract:
                attrs.append(('abstract', '1'))
            if node.is_final:
                attrs.append(('final', '1'))
        else:
            assert isinstance(node, ast.Interface)
            tag_name = 'interface'
        attrs.append(('glib:type-name', node.gtype_name))
        if node.get_type is not None:
            attrs.append(('glib:get-type', node.get_type))
        if node.glib_type_struct is not None:
            attrs.append(('glib:type-struct',
                          self._type_to_name(node.glib_type_struct)))
        if isinstance(node, ast.Class):
            if node.fundamental:
                attrs.append(('glib:fundamental', '1'))
            if node.ref_func:
                attrs.append(('glib:ref-func', node.ref_func))
            if node.unref_func:
                attrs.append(('glib:unref-func', node.unref_func))
            if node.set_value_func:
                attrs.append(('glib:set-value-func', node.set_value_func))
            if node.get_value_func:
                attrs.append(('glib:get-value-func', node.get_value_func))
        with self.tagcontext(tag_name, attrs):
            self._write_generic(node)
            if isinstance(node, ast.Class):
                for iface in sorted(node.interfaces):
                    self.write_tag('implements',
                                   [('name', self._type_to_name(iface))])
            if isinstance(node, ast.Interface):
                for iface in sorted(node.prerequisites):
                    self.write_tag('prerequisite',
                                   [('name', self._type_to_name(iface))])
            if isinstance(node, ast.Class):
                for method in sorted(node.constructors):
                    self._write_constructor(method)
            for method in sorted(node.static_methods):
                self._write_static_method(method)
            for vfunc in sorted(node.virtual_methods):
                self._write_vfunc(vfunc)
            for method in sorted(node.methods):
                self._write_method(method)
            for prop in sorted(node.properties):
                self._write_property(prop)
            for field in node.fields:
                self._write_field(field, node)
            for signal in sorted(node.signals):
                self._write_signal(signal)

    def _write_boxed(self, boxed):
        attrs = [('glib:name', boxed.name)]
        if boxed.c_symbol_prefix is not None:
            attrs.append(('c:symbol-prefix', boxed.c_symbol_prefix))
        self._append_registered(boxed, attrs)
        with self.tagcontext('glib:boxed', attrs):
            self._write_generic(boxed)
            for method in sorted(boxed.constructors):
                self._write_constructor(method)
            for method in sorted(boxed.methods):
                self._write_method(method)
            for method in sorted(boxed.static_methods):
                self._write_static_method(method)

    def _write_property(self, prop):
        attrs = [('name', prop.name)]
        self._append_version(prop, attrs)
        self._append_node_generic(prop, attrs)
        # Properties are assumed to be readable (see also generate.c)
        if not prop.readable:
            attrs.append(('readable', '0'))
        if prop.writable:
            attrs.append(('writable', '1'))
        if prop.construct:
            attrs.append(('construct', '1'))
        if prop.construct_only:
            attrs.append(('construct-only', '1'))
        if prop.transfer:
            attrs.append(('transfer-ownership', prop.transfer))
        if prop.setter:
            attrs.append(('setter', prop.setter))
        if prop.getter:
            attrs.append(('getter', prop.getter))
        if prop.default_value:
            attrs.append(('default-value', prop.default_value))
        with self.tagcontext('property', attrs):
            self._write_generic(prop)
            self._write_type(prop.type)

    def _write_vfunc(self, vf):
        attrs = []
        if vf.invoker:
            attrs.append(('invoker', vf.invoker))
        self._write_callable(vf, 'virtual-method', attrs)

    def _write_callback(self, callback):
        attrs = []
        if callback.ctype != callback.name:
            attrs.append(('c:type', callback.ctype))
        self._write_callable(callback, 'callback', attrs)

    def _write_record(self, record, extra_attrs=[]):
        is_gtype_struct = False
        attrs = list(extra_attrs)
        if record.name is not None:
            attrs.append(('name', record.name))
        if record.ctype is not None:  # the record might be anonymous
            attrs.append(('c:type', record.ctype))
        if record.disguised:
            attrs.append(('disguised', '1'))
        if record.opaque:
            attrs.append(('opaque', '1'))
        if record.pointer:
            attrs.append(('pointer', '1'))
        if record.foreign:
            attrs.append(('foreign', '1'))
        if record.is_gtype_struct_for is not None:
            is_gtype_struct = True
            attrs.append(('glib:is-gtype-struct-for',
                          self._type_to_name(record.is_gtype_struct_for)))
        if record.copy_func:
            attrs.append(('copy-function', record.copy_func))
        if record.free_func:
            attrs.append(('free-function', record.free_func))
        self._append_version(record, attrs)
        self._append_node_generic(record, attrs)
        self._append_registered(record, attrs)
        if record.c_symbol_prefix:
            attrs.append(('c:symbol-prefix', record.c_symbol_prefix))
        with self.tagcontext('record', attrs):
            self._write_generic(record)
            if record.fields:
                for field in record.fields:
                    self._write_field(field, record, is_gtype_struct)
            for method in sorted(record.constructors):
                self._write_constructor(method)
            for method in sorted(record.methods):
                self._write_method(method)
            for method in sorted(record.static_methods):
                self._write_static_method(method)

    def _write_union(self, union):
        attrs = []
        if union.name is not None:
            attrs.append(('name', union.name))
        if union.ctype is not None:  # the union might be anonymous
            attrs.append(('c:type', union.ctype))
        self._append_version(union, attrs)
        self._append_node_generic(union, attrs)
        self._append_registered(union, attrs)
        if union.c_symbol_prefix:
            attrs.append(('c:symbol-prefix', union.c_symbol_prefix))
        if union.copy_func:
            attrs.append(('copy-function', union.copy_func))
        if union.free_func:
            attrs.append(('free-function', union.free_func))
        with self.tagcontext('union', attrs):
            self._write_generic(union)
            if union.fields:
                for field in union.fields:
                    self._write_field(field, union)
            for method in sorted(union.constructors):
                self._write_constructor(method)
            for method in sorted(union.methods):
                self._write_method(method)
            for method in sorted(union.static_methods):
                self._write_static_method(method)

    def _write_field(self, field, parent, is_gtype_struct=False):
        if field.anonymous_node:
            if isinstance(field.anonymous_node, ast.Callback):
                attrs = [('name', field.name)]
                self._append_node_generic(field, attrs)
                with self.tagcontext('field', attrs):
                    self._write_generic(field)
                    self._write_callback(field.anonymous_node)
            elif isinstance(field.anonymous_node, ast.Record):
                self._write_record(field.anonymous_node)
            elif isinstance(field.anonymous_node, ast.Union):
                self._write_union(field.anonymous_node)
            else:
                raise AssertionError("Unknown field anonymous: %r" % (field.anonymous_node, ))
        else:
            attrs = [('name', field.name)]
            self._append_version(field, attrs)
            self._append_node_generic(field, attrs)
            # Fields are assumed to be read-only
            # (see also girparser.c and generate.c)
            if not field.readable:
                attrs.append(('readable', '0'))
            if field.writable:
                attrs.append(('writable', '1'))
            if field.bits:
                attrs.append(('bits', str(field.bits)))
            if field.private:
                attrs.append(('private', '1'))
            with self.tagcontext('field', attrs):
                self._write_generic(field)
                self._write_type(field.type, parent=parent)

    def _write_signal(self, signal):
        attrs = [('name', signal.name)]
        if signal.when:
            attrs.append(('when', signal.when))
        if signal.no_recurse:
            attrs.append(('no-recurse', '1'))
        if signal.detailed:
            attrs.append(('detailed', '1'))
        if signal.action:
            attrs.append(('action', '1'))
        if signal.no_hooks:
            attrs.append(('no-hooks', '1'))
        if signal.emitter:
            attrs.append(('emitter', signal.emitter))

        self._append_version(signal, attrs)
        self._append_node_generic(signal, attrs)
        with self.tagcontext('glib:signal', attrs):
            self._write_generic(signal)
            self._write_return_type(signal.retval)
            self._write_parameters(signal)
