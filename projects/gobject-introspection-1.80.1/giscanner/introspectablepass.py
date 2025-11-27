# -*- Mode: Python -*-
# Copyright (C) 2010 Red Hat, Inc.
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

from . import ast
from . import message
from .annotationparser import TAG_RETURNS


class IntrospectablePass(object):

    def __init__(self, transformer, blocks):
        self._transformer = transformer
        self._namespace = transformer.namespace
        self._blocks = blocks

    # Public API

    def validate(self):
        self._namespace.walk(self._introspectable_alias_analysis)
        self._namespace.walk(self._propagate_callable_skips)
        self._namespace.walk(self._analyze_node)
        self._namespace.walk(self._introspectable_callable_analysis)
        self._namespace.walk(self._introspectable_callable_analysis)
        self._namespace.walk(self._introspectable_property_analysis)
        self._namespace.walk(self._introspectable_pass3)
        self._namespace.walk(self._remove_non_reachable_backcompat_copies)
        self._namespace.walk(self._introspectable_symbol_collisions)

    def _parameter_warning(self, parent, param, text, position=None):
        # Suppress VFunctions and Callbacks warnings for now
        # they cause more problems then they are worth
        if isinstance(parent, (ast.VFunction, ast.Callback)):
            return

        block = None
        if hasattr(parent, 'symbol'):
            prefix = '%s: ' % (parent.symbol, )
            block = self._blocks.get(parent.symbol)
            if block:
                position = block.position
        else:
            prefix = ''
        if isinstance(param, ast.Parameter):
            context = "argument %s: " % (param.argname, )
        else:
            context = "return value: "
            if block:
                return_tag = block.tags.get(TAG_RETURNS)
                if return_tag:
                    position = return_tag.position
        message.warn_node(parent, prefix + context + text,
                          positions=position)

    def _introspectable_param_analysis(self, parent, node):
        is_return = isinstance(node, ast.Return)
        is_parameter = isinstance(node, ast.Parameter)
        assert is_return or is_parameter

        if node.type.target_giname is not None:
            target = self._transformer.lookup_typenode(node.type)
        else:
            target = None

        if node.skip:
            return

        if not node.type.resolved:
            self._parameter_warning(parent, node,
                                    "Unresolved type: '%s'" % (node.type.unresolved_string, ))
            parent.introspectable = False
            return

        if isinstance(node.type, ast.Varargs):
            parent.introspectable = False
            return

        if (isinstance(node.type, (ast.List, ast.Array))
        and node.type.element_type == ast.TYPE_ANY):
            self._parameter_warning(parent, node, "Missing (element-type) annotation")
            parent.introspectable = False
            return

        if (is_parameter
        and isinstance(target, ast.Callback)
        and node.type.target_giname not in ('GLib.DestroyNotify', 'Gio.AsyncReadyCallback')
        and node.scope is None):
            self._parameter_warning(
                parent,
                node,
                "Missing (scope) annotation for callback without "
                "GDestroyNotify (valid: %s, %s, %s)" % (ast.PARAM_SCOPE_CALL, ast.PARAM_SCOPE_ASYNC, ast.PARAM_SCOPE_FOREVER))

            parent.introspectable = False
            return

        if is_return and isinstance(target, ast.Callback):
            self._parameter_warning(parent, node, "Callbacks cannot be return values; use (skip)")
            parent.introspectable = False
            return

        if (is_return
        and isinstance(target, (ast.Record, ast.Union))
        and target.get_type is None
        and (target.copy_func is None or target.free_func is None)
        and not target.foreign):
            if node.transfer != ast.PARAM_TRANSFER_NONE:
                self._parameter_warning(
                    parent, node,
                    "Invalid non-constant return of bare structure or union; "
                    "register as boxed type, add (copy-func) and (free-func), "
                    "or (skip)")
                parent.introspectable = False
            return

        if node.transfer is None:
            self._parameter_warning(parent, node, "Missing (transfer) annotation")
            parent.introspectable = False
            return

    def _type_is_introspectable(self, typeval, warn=False):
        if not typeval.resolved:
            return False
        if isinstance(typeval, ast.TypeUnknown):
            return False
        if isinstance(typeval, (ast.Array, ast.List)):
            return self._type_is_introspectable(typeval.element_type)
        elif isinstance(typeval, ast.Map):
            return (self._type_is_introspectable(typeval.key_type)
                    and self._type_is_introspectable(typeval.value_type))
        if typeval.target_foreign:
            return True
        if typeval.target_fundamental:
            if typeval.is_equiv(ast.TYPE_VALIST):
                return False
            # These are not introspectable pending us adding
            # larger type tags to the typelib (in theory these could
            # be 128 bit or larger)
            elif typeval.is_equiv((ast.TYPE_LONG_LONG, ast.TYPE_LONG_ULONG, ast.TYPE_LONG_DOUBLE)):
                return False
            else:
                return True
        target = self._transformer.lookup_typenode(typeval)
        if not target:
            return False
        return target.introspectable and (not target.skip)

    def _propagate_parameter_skip(self, parent, node):
        if node.type.target_giname is not None:
            target = self._transformer.lookup_typenode(node.type)
            if target is None:
                return
        else:
            return

        if target.skip:
            parent.skip = True

    def _introspectable_alias_analysis(self, obj, stack):
        if isinstance(obj, ast.Alias):
            if not self._type_is_introspectable(obj.target):
                obj.introspectable = False
        return True

    def _propagate_callable_skips(self, obj, stack):
        if isinstance(obj, ast.Callable):
            for param in obj.parameters:
                self._propagate_parameter_skip(obj, param)
            self._propagate_parameter_skip(obj, obj.retval)
        return True

    def _analyze_node(self, obj, stack):
        if obj.skip:
            return False
        # Our first pass for scriptability
        if isinstance(obj, ast.Callable):
            for param in obj.parameters:
                self._introspectable_param_analysis(obj, param)
            self._introspectable_param_analysis(obj, obj.retval)
        if isinstance(obj, (ast.Class, ast.Interface, ast.Record, ast.Union)):
            for field in obj.fields:
                if field.type:
                    if not self._type_is_introspectable(field.type):
                        field.introspectable = False
        return True

    def _introspectable_callable_analysis(self, obj, stack):
        if obj.skip:
            return False
        # Propagate introspectability of parameters to entire functions
        if isinstance(obj, ast.Callable):
            for param in obj.parameters:
                if not self._type_is_introspectable(param.type):
                    obj.introspectable = False
                    return True
            if not self._type_is_introspectable(obj.retval.type):
                obj.introspectable = False
                return True
        if isinstance(obj, ast.Function) and obj.is_inline:
            obj.introspectable = False
            return True
        if isinstance(obj, ast.Signal):
            if obj.emitter is None:
                return False
            parent = stack[0]
            for method in parent.methods:
                if method.name != obj.emitter:
                    continue
                if not obj.retval.type.is_equiv(method.retval.type):
                    self._parameter_warning(
                        parent,
                        obj,
                        "Emitter method %s for signal %s::%s does not have the "
                        "same return value type" % (method.symbol, parent.name, obj.name))
                    obj.emitter = None
                    return False
                n_emitter_params = len(method.parameters)
                n_signal_params = len(obj.parameters)
                if n_emitter_params != n_signal_params:
                    self._parameter_warning(
                        parent,
                        obj,
                        "Emitter method %s for signal %s::%s does not have the "
                        "same number of arguments (expected: %d)" % (method.symbol, parent.name, obj.name, n_signal_params))
                    obj.emitter = None
                    return False
                for idx, signal_param in enumerate(obj.parameters):
                    method_param = method.parameters[idx + 1]
                    if signal_param.type.is_equiv(method_param.type):
                        self._parameter_warning(
                            parent,
                            obj,
                            "Emitter method %s for signal %s::%s does not have the "
                            "same type of arguments" % (method.symbol, parent.name, obj.name))
                        obj.emitter = None
                        return False
        return True

    def _introspectable_property_analysis(self, obj, stack):
        if obj.skip:
            return False
        if isinstance(obj, (ast.Class, ast.Interface)):
            for prop in obj.properties:
                if not self._type_is_introspectable(prop.type):
                    prop.introspectable = False
                    prop.setter = None
                    prop.getter = None
            for method in obj.methods:
                set_property = method.set_property
                if set_property is not None:
                    for prop in obj.properties:
                        if prop.name == set_property and not prop.introspectable:
                            method.set_property = None
                            break
                get_property = method.get_property
                if get_property is not None:
                    for prop in obj.properties:
                        if prop.name == get_property and not prop.introspectable:
                            method.get_property = None
                            break
        return True

    def _introspectable_pass3(self, obj, stack):
        if obj.skip:
            return False
        # Propagate introspectability for fields
        if isinstance(obj, (ast.Class, ast.Interface, ast.Record, ast.Union)):
            for field in obj.fields:
                if field.anonymous_node:
                    if not field.anonymous_node.introspectable:
                        field.introspectable = False
                else:
                    if not self._type_is_introspectable(field.type):
                        field.introspectable = False
        # Propagate introspectability for properties
        if isinstance(obj, (ast.Class, ast.Interface)):
            for sig in obj.signals:
                self._introspectable_callable_analysis(sig, [obj])
        return True

    def _remove_non_reachable_backcompat_copies(self, obj, stack):
        if obj.skip:
            return False
        if (isinstance(obj, ast.Function) and obj.moved_to is not None):
            # remove functions that are not introspectable
            if not obj.introspectable:
                obj.internal_skipped = True
        return True

    def _property_warning(self, parent, prop, text, position=None):
        context = "property %s:%s: " % (parent.name, prop.name, )
        message.strict_node(parent, context + text, positions=position)

    def _property_signal_collision(self, obj, prop):
        for s in obj.signals:
            if s.skip or not s.introspectable:
                continue
            if s.name.replace('-', '_') == prop.name.replace('-', '_'):
                self._property_warning(obj, prop, "Properties cannot have the same name as signals")
        return False

    def _property_method_collision(self, obj, prop):
        for m in obj.methods:
            if m.skip or not m.introspectable:
                continue
            if m.name == prop.name.replace('-', '_'):
                self._property_warning(obj, prop, "Properties cannot have the same name as methods")
        return False

    def _property_vfunc_collision(self, obj, prop):
        for vfunc in obj.virtual_methods:
            if vfunc.skip or not vfunc.introspectable:
                continue
            if vfunc.name == prop.name.replace('-', '_'):
                self._property_warning(obj, prop, "Properties cannot have the same name as virtual methods")
        return False

    def _introspectable_symbol_collisions(self, obj, stack):
        if obj.skip:
            return False
        if isinstance(obj, (ast.Class, ast.Interface)):
            for prop in obj.properties:
                if prop.skip or not prop.introspectable:
                    continue
                self._property_signal_collision(obj, prop)
                self._property_method_collision(obj, prop)
                self._property_vfunc_collision(obj, prop)
        return True
