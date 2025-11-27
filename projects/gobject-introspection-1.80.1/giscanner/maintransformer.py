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

import re

from . import ast
from . import message
from .annotationparser import (TAG_DEPRECATED, TAG_SINCE, TAG_STABILITY, TAG_RETURNS)
from .annotationparser import (
    ANN_ALLOW_NONE,
    ANN_ARRAY,
    ANN_ATTRIBUTES,
    ANN_CLOSURE,
    ANN_CONSTRUCTOR,
    ANN_COPY_FUNC,
    ANN_DEFAULT_VALUE,
    ANN_DESTROY,
    ANN_ELEMENT_TYPE,
    ANN_EMITTER,
    ANN_FOREIGN,
    ANN_FREE_FUNC,
    ANN_GET_PROPERTY,
    ANN_GET_VALUE_FUNC,
    ANN_GETTER,
    ANN_IN,
    ANN_INOUT,
    ANN_METHOD,
    ANN_OUT,
    ANN_REF_FUNC,
    ANN_RENAME_TO,
    ANN_SCOPE,
    ANN_SET_PROPERTY,
    ANN_SET_VALUE_FUNC,
    ANN_SETTER,
    ANN_SKIP,
    ANN_TRANSFER,
    ANN_TYPE,
    ANN_UNREF_FUNC,
    ANN_VALUE,
    ANN_VFUNC,
    ANN_NULLABLE,
    ANN_OPTIONAL,
    ANN_NOT,
    ANN_FINISH_FUNC,
    ANN_SYNC_FUNC,
    ANN_ASYNC_FUNC,
)
from .annotationparser import (
    OPT_ARRAY_FIXED_SIZE,
    OPT_ARRAY_LENGTH,
    OPT_ARRAY_ZERO_TERMINATED,
    OPT_OUT_CALLEE_ALLOCATES,
    OPT_OUT_CALLER_ALLOCATES,
    OPT_TRANSFER_CONTAINER,
    OPT_TRANSFER_FLOATING,
    OPT_TRANSFER_NONE,
)

from .utils import to_underscores_noprefix


class MainTransformer(object):

    def __init__(self, transformer, blocks):
        self._transformer = transformer
        self._blocks = blocks
        self._namespace = transformer.namespace
        self._uscore_type_names = {}

    # Public API

    def transform(self):
        if not self._namespace.names:
            message.error('Namespace is empty; likely causes are:\n'
                          '* Not including .h files to be scanned\n'
                          '* Broken --identifier-prefix')

        # Some initial namespace surgery
        self._namespace.walk(self._pass_fixup_hidden_fields)

        # We have a rough tree which should have most of of the types
        # we know about.  Let's attempt closure; walk over all of the
        # Type() types and see if they match up with something.
        self._namespace.walk(self._pass_type_resolution)

        # Read in annotations needed early
        self._namespace.walk(self._pass_read_annotations_early)

        # Determine some default values for transfer etc.
        # based on the current tree.
        self._namespace.walk(self._pass_callable_defaults)

        # Read in most annotations now.
        self._namespace.walk(self._pass_read_annotations)

        # Now that we have associated doc SECTIONs to classes,
        # add the unused section blocks as standalone nodes.
        self._add_standalone_doc_sections()

        # Now that we've possibly seen more types from annotations,
        # do another type resolution pass.
        self._namespace.walk(self._pass_type_resolution)

        # Generate a reverse mapping "bar_baz" -> BarBaz
        for node in self._namespace.values():
            if isinstance(node, ast.Registered) and node.get_type is not None:
                self._uscore_type_names[node.c_symbol_prefix] = node
            elif isinstance(node, (ast.Record, ast.Union)):
                uscored = to_underscores_noprefix(node.name).lower()
                self._uscore_type_names[uscored] = node

        for node in list(self._namespace.values()):
            if isinstance(node, ast.Function):
                # Discover which toplevel functions are actually methods
                self._pair_function(node)
            if isinstance(node, (ast.Class, ast.Interface)):
                self._pair_class_virtuals(node)
                self._pair_property_accessors(node)

        # Some annotations need to be post function pairing
        self._namespace.walk(self._pass_read_annotations2)

        # Another type resolution pass after we've parsed virtuals, etc.
        self._namespace.walk(self._pass_type_resolution)

        self._namespace.walk(self._pass3)

        # TODO - merge into pass3
        self._pair_quarks_with_enums()

    # Private

    def _add_standalone_doc_sections(self):
        for block_name, block in self._blocks.items():
            if block_name.startswith("SECTION:") and block.description:
                node = ast.DocSection(block_name[8:])
                node.doc = block.description
                node.doc_position = block.position
                self._namespace.append(node)

    def _pass_fixup_hidden_fields(self, node, chain):
        """Hide all callbacks starting with _; the typical
        usage is void (*_gtk_reserved1)(void);"""
        if isinstance(node, (ast.Class, ast.Interface, ast.Record, ast.Union)):
            for field in node.fields:
                if (field
                and field.name is not None
                and field.name.startswith('_')
                and field.anonymous_node is not None
                and isinstance(field.anonymous_node, ast.Callback)):
                    field.introspectable = False
        return True

    def _get_validate_parameter_name(self, parent, param_name, origin):
        try:
            param = parent.get_parameter(param_name)
        except ValueError:
            param = None
        if param is None:
            if isinstance(origin, ast.Parameter):
                origin_name = 'parameter %s' % (origin.argname, )
            else:
                origin_name = 'return value'
            message.log_node(
                message.FATAL, parent,
                "can't find parameter %s referenced by %s of '%s'"
                % (param_name, origin_name, parent.name))

        return param.argname

    def _get_validate_field_name(self, parent, field_name, origin):
        try:
            field = parent.get_field(field_name)
        except ValueError:
            field = None
        if field is None:
            origin_name = 'field %s' % (origin.name, )
            message.log_node(
                message.FATAL, parent,
                "can't find field %s referenced by %s of '%s'"
                % (field_name, origin_name, parent.name))

        return field.name

    def _apply_annotation_rename_to(self, node, chain, block):
        if not block:
            return
        rename_to = block.annotations.get(ANN_RENAME_TO)
        if not rename_to:
            return
        rename_to = rename_to[0]
        target = self._namespace.get_by_symbol(rename_to)
        if not target:
            message.warn_node(node,
                "Can't find symbol '%s' referenced by \"rename-to\" annotation" % (rename_to, ))
        elif target.shadowed_by:
            message.warn_node(node,
                "Function '%s' already shadowed by '%s', can't overwrite "
                "with '%s'" % (target.symbol,
                             target.shadowed_by,
                             rename_to))
        elif target.shadows:
            message.warn_node(node,
                "Function '%s' already shadows '%s', can't multiply shadow "
                "with '%s'" % (target.symbol,
                             target.shadows,
                             rename_to))
        else:
            target.shadowed_by = node.name
            node.shadows = target.name

    def _apply_annotations_function(self, node, chain):
        block = self._blocks.get(node.symbol)
        self._apply_annotations_callable(node, chain, block)

    def _apply_annotations_function_macro(self, node, chain):
        block = self._blocks.get(node.symbol)
        for param in node.parameters:
            if block:
                doc_param = block.params.get(param.argname)
                if doc_param:
                    param.doc = doc_param.description
                    param.doc_position = doc_param.position
        self._apply_annotations_annotated(node, block)

    def _pass_read_annotations_early(self, node, chain):
        if isinstance(node, ast.Record):
            if node.ctype is not None:
                block = self._blocks.get(node.ctype)
            else:
                block = self._blocks.get(node.c_name)
            self._apply_annotations_annotated(node, block)
        return True

    def _pass_callable_defaults(self, node, chain):
        if isinstance(node, (ast.Callable, ast.Signal)):
            for param in node.parameters:
                if param.transfer is None:
                    param.transfer = self._get_transfer_default(node, param)
            if node.retval.transfer is None:
                node.retval.transfer = self._get_transfer_default(node, node.retval)
        return True

    def _get_annotation_name(self, node):
        if isinstance(node, (ast.Class, ast.Interface, ast.Record,
                             ast.Union, ast.Enum, ast.Bitfield,
                             ast.Callback, ast.Alias, ast.Constant)):
            if node.ctype is not None:
                return node.ctype
            elif isinstance(node, ast.Registered) and node.gtype_name is not None:
                return node.gtype_name
            return node.c_name
        raise AssertionError("Unhandled node '%s'" % (node, ))

    def _get_block(self, node):
        return self._blocks.get(self._get_annotation_name(node))

    def _pass_read_annotations(self, node, chain):
        if not node.namespace:
            return False
        if isinstance(node, ast.Alias):
            self._apply_annotations_alias(node, chain)
        if isinstance(node, ast.Function):
            self._apply_annotations_function(node, chain)
        if isinstance(node, ast.FunctionMacro):
            self._apply_annotations_function_macro(node, chain)
        if isinstance(node, ast.Callback):
            self._apply_annotations_callable(node, chain, block=self._get_block(node))
        if isinstance(node, (ast.Class, ast.Interface, ast.Union, ast.Enum,
                             ast.Bitfield, ast.Callback)):
            self._apply_annotations_annotated(node, self._get_block(node))
        if isinstance(node, (ast.Enum, ast.Bitfield)):
            self._apply_annotations_enum_members(node, self._get_block(node))
        if isinstance(node, (ast.Class, ast.Interface, ast.Record, ast.Union)):
            block = self._get_block(node)
            for field in node.fields:
                self._apply_annotations_field(node, block, field)
            name = self._get_annotation_name(node)
            section_name = 'SECTION:%s' % (name.lower(), )
            # We pop it from our blocks so that we can serialize leftover
            # SECTIONs as standalone nodes
            block = self._blocks.pop(section_name, None)
            self._apply_annotations_annotated(node, block)
        if isinstance(node, (ast.Class, ast.Interface)):
            for prop in node.properties:
                self._apply_annotations_property(node, prop)
            for sig in node.signals:
                self._apply_annotations_signal(node, sig)
        if isinstance(node, ast.Class):
            block = self._get_block(node)
            if block:
                annotation = block.annotations.get(ANN_UNREF_FUNC)
                node.unref_func = annotation[0] if annotation else None
                annotation = block.annotations.get(ANN_REF_FUNC)
                node.ref_func = annotation[0] if annotation else None
                annotation = block.annotations.get(ANN_SET_VALUE_FUNC)
                node.set_value_func = annotation[0] if annotation else None
                annotation = block.annotations.get(ANN_GET_VALUE_FUNC)
                node.get_value_func = annotation[0] if annotation else None
        if isinstance(node, ast.Constant):
            self._apply_annotations_constant(node)
        if isinstance(node, (ast.Record, ast.Union)):
            block = self._get_block(node)
            if block:
                annotation = block.annotations.get(ANN_COPY_FUNC)
                node.copy_func = annotation[0] if annotation else None
                annotation = block.annotations.get(ANN_FREE_FUNC)
                node.free_func = annotation[0] if annotation else None
        return True

    def _adjust_container_type(self, parent, node, annotations):
        if ANN_ARRAY in annotations:
            self._apply_annotations_array(parent, node, annotations)
        elif ANN_ELEMENT_TYPE in annotations:
            self._apply_annotations_element_type(parent, node, annotations)

        if isinstance(node.type, ast.Array):
            self._check_array_element_type(node.type, annotations)

    def _resolve(self, type_str, type_node=None, node=None, parent=None):
        def grab_one(type_str, resolver, top_combiner, combiner):
            """Return a complete type, and the trailing string part after it.
            Use resolver() on each identifier, and combiner() on the parts of
            each complete type. (top_combiner is used on the top-most type.)"""
            bits = re.split(r'([,<>()])', type_str, 1)
            first, sep, rest = [bits[0], '', ''] if (len(bits) == 1) else bits
            args = [resolver(first)]
            if sep == '<' or sep == '(':
                lastsep = '>' if (sep == '<') else ')'
                while sep != lastsep:
                    next, rest = grab_one(rest, resolver, combiner, combiner)
                    args.append(next)
                    sep, rest = rest[0], rest[1:]
            else:
                rest = sep + rest
            return top_combiner(*args), rest

        def resolver(ident):
            res = self._transformer.create_type_from_user_string(ident)
            return res

        def combiner(base, *rest):
            if not rest:
                return base
            if isinstance(base, ast.List) and len(rest) == 1:
                return ast.List(base.name, *rest)
            elif isinstance(base, ast.Array) and len(rest) == 1:
                base.element_type = rest[0]
                return base
            elif isinstance(base, ast.Map) and len(rest) == 2:
                return ast.Map(*rest)
            message.warn(
                "Too many parameters in type specification '%s'" % (type_str, ))
            return base

        def top_combiner(base, *rest):
            if type_node is not None and isinstance(type_node, ast.Type):
                base.is_const = type_node.is_const
            return combiner(base, *rest)

        result, rest = grab_one(type_str, resolver, top_combiner, combiner)
        if rest:
            message.warn("Trailing components in type specification '%s'" % (
                type_str, ))

        if not result.resolved:
            position = None
            if parent is not None and isinstance(parent, ast.Function):
                text = parent.symbol
                position = self._get_position(parent, node)
            else:
                text = type_str
            message.warn_node(parent, "%s: Unknown type: '%s'" %
                              (text, type_str), positions=position)
        return result

    def _resolve_toplevel(self, type_str, type_node=None, node=None, parent=None):
        """Like _resolve(), but attempt to preserve more attributes of original type."""
        result = self._resolve(type_str, type_node=type_node, node=node, parent=parent)
        # If we replace a node with a new type (such as an annotated) we
        # might lose the ctype from the original node.
        if type_node is not None:
            result.ctype = type_node.ctype
            result.complete_ctype = type_node.complete_ctype
        return result

    def _get_position(self, func, param):
        block = self._blocks.get(func.symbol)
        if block:
            if isinstance(param, ast.Parameter):
                part = block.params.get(param.argname)
            elif isinstance(param, ast.Return):
                part = block.tags.get(TAG_RETURNS)
            else:
                part = None

            if part.position:
                return part.position

        return block.position

    def _check_array_element_type(self, array, annotations):
        array_type = array.array_type
        element_type = array.element_type

        # GPtrArrays are allowed to contain non basic types
        # (except enums and flags) or basic types that are
        # as big as a gpointer
        if array_type == ast.Array.GLIB_PTRARRAY:
            if ((element_type in ast.BASIC_GIR_TYPES and element_type not in ast.POINTER_TYPES)
            or isinstance(element_type, (ast.Enum, ast.Bitfield))):
                message.warn("invalid (element-type) for a GPtrArray, "
                             "must be a pointer", annotations.position)

        if (array_type == ast.Array.GLIB_BYTEARRAY
        and element_type not in [ast.TYPE_UINT8, ast.TYPE_INT8, ast.TYPE_CHAR]):
            message.warn("invalid (element-type) for a GByteArray, "
                         "must be one of guint8, gint8 or gchar",
                         annotations.position)

    def _apply_annotations_array(self, parent, node, annotations):
        element_type_options = annotations.get(ANN_ELEMENT_TYPE)
        if element_type_options:
            element_type_node = self._resolve(element_type_options[0],
                                              node.type, node, parent)
        elif isinstance(node.type, ast.Array):
            element_type_node = node.type.element_type
        else:
            # We're assuming here that Foo* with an (array) annotation
            # and no (element-type) means array of Foo
            element_type_node = node.type.clone()
            # The element's ctype is the array's dereferenced
            if element_type_node.ctype is not None and element_type_node.ctype.endswith('*'):
                element_type_node.ctype = element_type_node.ctype[:-1]

        if isinstance(node.type, ast.Array):
            array_type = node.type.array_type
        else:
            array_type = None

        array_options = annotations.get(ANN_ARRAY)
        container_type = ast.Array(array_type, element_type_node, ctype=node.type.ctype,
                                   complete_ctype=node.type.complete_ctype,
                                   is_const=node.type.is_const)
        if array_options.get(OPT_ARRAY_ZERO_TERMINATED, '0') == '0':
            container_type.zeroterminated = False
        else:
            if (OPT_ARRAY_ZERO_TERMINATED in array_options
            or array_options.get(OPT_ARRAY_ZERO_TERMINATED) == '1'):
                container_type.zeroterminated = True
            else:
                container_type.zeroterminated = False

        length = array_options.get(OPT_ARRAY_LENGTH)
        if length:
            if isinstance(parent, ast.Compound):
                paramname = self._get_validate_field_name(parent, length, node)
            else:
                paramname = self._get_validate_parameter_name(parent, length, node)
                if paramname:
                    param = parent.get_parameter(paramname)
                    param.direction = node.direction
                    if param.direction == ast.PARAM_DIRECTION_OUT:
                        param.transfer = ast.PARAM_TRANSFER_FULL
            if paramname:
                container_type.length_param_name = paramname
        fixed = array_options.get(OPT_ARRAY_FIXED_SIZE)
        if fixed:
            try:
                container_type.size = int(fixed)
            except (TypeError, ValueError):
                # Already warned in annotationparser.py
                return
        node.type = container_type

    def _apply_annotations_element_type(self, parent, node, annotations):
        element_type_options = annotations.get(ANN_ELEMENT_TYPE)
        if element_type_options is None:
            return

        if isinstance(node.type, ast.List):
            if len(element_type_options) != 1:
                message.warn(
                    '"element-type" annotation for a list must have exactly '
                    'one option, not %d options' % (len(element_type_options), ),
                    annotations.position)
                return
            node.type.element_type = self._resolve(element_type_options[0],
                                                   node.type, node, parent)
        elif isinstance(node.type, ast.Map):
            if len(element_type_options) != 2:
                message.warn(
                    '"element-type" annotation for a hash table must have exactly '
                    'two options, not %d option(s)' % (len(element_type_options), ),
                    annotations.position)
                return
            node.type.key_type = self._resolve(element_type_options[0],
                                               node.type, node, parent)
            node.type.value_type = self._resolve(element_type_options[1],
                                                 node.type, node, parent)
        elif isinstance(node.type, ast.Array):
            if len(element_type_options) != 1:
                message.warn(
                    '"element-type" annotation for an array must have exactly '
                    'one option, not %d options' % (len(element_type_options), ),
                    annotations.position)
                return
            node.type.element_type = self._resolve(element_type_options[0],
                                                   node.type, node, parent)
        else:
            message.warn(
                "Unknown container %r for element-type annotation" % (node.type, ),
                annotations.position)

    def _get_transfer_default_param(self, parent, node):
        if node.direction in [ast.PARAM_DIRECTION_INOUT,
                              ast.PARAM_DIRECTION_OUT]:
            if node.caller_allocates:
                return ast.PARAM_TRANSFER_NONE
            return ast.PARAM_TRANSFER_FULL
        return ast.PARAM_TRANSFER_NONE

    def _get_transfer_default_returntype_basic(self, typeval):
        if (typeval.is_equiv(ast.BASIC_GIR_TYPES)
        or typeval.is_const
        or typeval.is_equiv((ast.TYPE_ANY, ast.TYPE_NONE))):
            return ast.PARAM_TRANSFER_NONE
        elif typeval.is_equiv(ast.TYPE_STRING):
            # Non-const strings default to FULL
            return ast.PARAM_TRANSFER_FULL
        elif typeval.target_fundamental:
            # This looks like just GType right now
            return None
        return None

    def _is_gi_subclass(self, typeval, supercls_type):
        cls = self._transformer.lookup_typenode(typeval)
        assert cls, str(typeval)
        supercls = self._transformer.lookup_typenode(supercls_type)
        assert supercls
        if cls is supercls:
            return True
        if cls.parent_type and cls.parent_type.target_giname != 'GObject.Object':
            return self._is_gi_subclass(cls.parent_type, supercls_type)
        return False

    def _get_transfer_default_return(self, parent, node):
        typeval = node.type
        basic = self._get_transfer_default_returntype_basic(typeval)
        if basic:
            return basic
        if not typeval.target_giname:
            return None
        target = self._transformer.lookup_typenode(typeval)
        if isinstance(target, ast.Alias):
            return self._get_transfer_default_returntype_basic(target.target)
        elif (isinstance(target, ast.Boxed)
              or (isinstance(target, (ast.Record, ast.Union))
                  and (target.gtype_name is not None or target.foreign))):
            return ast.PARAM_TRANSFER_FULL
        elif isinstance(target, (ast.Enum, ast.Bitfield)):
            return ast.PARAM_TRANSFER_NONE
        # Handle constructors specially here
        elif isinstance(parent, ast.Function) and parent.is_constructor:
            if isinstance(target, ast.Class):
                initially_unowned_type = ast.Type(target_giname='GObject.InitiallyUnowned')
                try:
                    initially_unowned = self._transformer.lookup_typenode(initially_unowned_type)
                except KeyError:
                    message.error_node(node, "constructor found but GObject is not in includes")
                    return None
                if initially_unowned and self._is_gi_subclass(typeval, initially_unowned_type):
                    return ast.PARAM_TRANSFER_NONE
                else:
                    return ast.PARAM_TRANSFER_FULL
            elif isinstance(target, (ast.Record, ast.Union)):
                return ast.PARAM_TRANSFER_FULL
            else:
                raise AssertionError("Invalid constructor")
        elif isinstance(target, (ast.Class, ast.Record, ast.Union)):
            # Explicitly no default for these
            return None
        else:
            return None

    def _get_transfer_default(self, parent, node):
        if node.type.is_equiv(ast.TYPE_NONE) or isinstance(node.type, ast.Varargs):
            return ast.PARAM_TRANSFER_NONE
        elif isinstance(node, ast.Parameter):
            return self._get_transfer_default_param(parent, node)
        elif isinstance(node, ast.Return):
            return self._get_transfer_default_return(parent, node)
        elif isinstance(node, ast.Field):
            return ast.PARAM_TRANSFER_NONE
        elif isinstance(node, ast.Property):
            return ast.PARAM_TRANSFER_NONE
        else:
            raise AssertionError(node)

    def _is_pointer_type(self, node, annotations):
        if (not isinstance(node, ast.Return) and
                node.direction in (ast.PARAM_DIRECTION_OUT,
                                   ast.PARAM_DIRECTION_INOUT)):
            return True

        target = self._transformer.lookup_typenode(node.type)
        target = self._transformer.resolve_aliases(target)
        target = node.type if target is None else target

        return (not isinstance(target, ast.Type) or
                target not in ast.BASIC_TYPES or
                target.ctype.endswith('*'))

    def _apply_transfer_annotation(self, parent, node, annotations):
        transfer_annotation = annotations.get(ANN_TRANSFER)
        if not transfer_annotation or len(transfer_annotation) != 1:
            return

        transfer = transfer_annotation[0]

        target = self._transformer.lookup_typenode(node.type)
        target = self._transformer.resolve_aliases(target)
        target = node.type if target is None else target
        node_type = target if isinstance(target, ast.Type) else node.type

        if transfer == OPT_TRANSFER_FLOATING:
            transfer = OPT_TRANSFER_NONE

            if (not isinstance(target, (ast.Class, ast.Interface))
                    and node_type.target_giname != 'GLib.Variant'
                    and node_type.target_giname != 'GObject.Closure'):
                message.warn('invalid "transfer" annotation for {0}: '
                             'only valid for object, GVariant and GClosure types'.format(target),
                             annotations.position)
                return

        elif transfer == OPT_TRANSFER_CONTAINER:
            if (ANN_ARRAY not in annotations and
                    not isinstance(target, (ast.Array, ast.List, ast.Map))):
                message.warn('invalid "transfer" annotation for {0}: '
                             'only valid for container types'.format(target),
                             annotations.position)
                return

        elif (not self._is_pointer_type(node, annotations) and
              node_type not in (ast.TYPE_STRING, ast.TYPE_FILENAME) and
              not isinstance(target, (ast.Array, ast.List, ast.Map,
                                      ast.Record, ast.Compound, ast.Boxed,
                                      ast.Class, ast.Interface))):
            message.warn('invalid "transfer" annotation for {0}: '
                         'only valid for array, struct, union, boxed, '
                         'object and interface types'.format(target),
                         annotations.position)
            return

        node.transfer = transfer

    def _apply_annotations_param_ret_common(self, parent, node, tag):
        annotations = tag.annotations if tag else {}

        type_annotation = annotations.get(ANN_TYPE)
        if type_annotation:
            node.type = self._resolve_toplevel(type_annotation[0],
                                               node.type, node, parent)

        caller_allocates = False
        annotated_direction = None
        if ANN_INOUT in annotations:
            annotated_direction = ast.PARAM_DIRECTION_INOUT
        elif ANN_OUT in annotations:
            annotated_direction = ast.PARAM_DIRECTION_OUT

            options = annotations[ANN_OUT]
            if len(options) == 0:
                if node.type.target_giname and node.type.ctype:
                    target = self._transformer.lookup_giname(node.type.target_giname)
                    target = self._transformer.resolve_aliases(target)
                    has_double_indirection = '**' in node.type.ctype
                    is_structure_or_union = isinstance(target, (ast.Record, ast.Union))
                    caller_allocates = (not has_double_indirection and is_structure_or_union)
                else:
                    caller_allocates = False
            else:
                option = options[0]
                if option == OPT_OUT_CALLER_ALLOCATES:
                    caller_allocates = True
                elif option == OPT_OUT_CALLEE_ALLOCATES:
                    caller_allocates = False
        elif ANN_IN in annotations:
            annotated_direction = ast.PARAM_DIRECTION_IN

        if (annotated_direction is not None) and (annotated_direction != node.direction):
            node.direction = annotated_direction
            node.caller_allocates = caller_allocates
            # Also reset the transfer default if we're toggling direction
            node.transfer = self._get_transfer_default(parent, node)

        self._apply_transfer_annotation(parent, node, annotations)
        self._adjust_container_type(parent, node, annotations)

        # gpointer parameters and return values are always nullable unless:
        #  - annotated with (type) and not also with (nullable); or
        #  - annotated with (element-type) and not also with (nullable); or
        #  - annotated (not nullable)
        # See: https://bugzilla.gnome.org/show_bug.cgi?id=719966#c22
        if node.type.is_equiv(ast.TYPE_ANY):
            node.nullable = True
        if ANN_NULLABLE in annotations:
            if self._is_pointer_type(node, annotations):
                node.nullable = True
                node.not_nullable = False
            else:
                message.warn('invalid "nullable" annotation: '
                             'only valid for pointer types and out parameters',
                             annotations.position)

        if ANN_OPTIONAL in annotations:
            if (not isinstance(node, ast.Return) and
                    node.direction in [ast.PARAM_DIRECTION_OUT,
                                       ast.PARAM_DIRECTION_INOUT]):
                node.optional = True
            else:
                message.warn('invalid "optional" annotation: '
                             'only valid for out and inout parameters',
                             annotations.position)

        if ANN_ALLOW_NONE in annotations:
            if (node.direction == ast.PARAM_DIRECTION_OUT and
                    not isinstance(node, ast.Return)):
                node.optional = True
            elif self._is_pointer_type(node, annotations):
                node.nullable = True
            else:
                message.warn('invalid "allow-none" annotation: '
                             'only valid for pointer types and out parameters',
                             annotations.position)

        if (node.direction != ast.PARAM_DIRECTION_OUT and
                (node.type.target_giname == 'Gio.AsyncReadyCallback' or
                 node.type.target_giname == 'Gio.Cancellable')):
            node.nullable = True

        # Final override for nullability
        if ANN_NOT in annotations:
            node.nullable = False
            node.not_nullable = True

        if tag and tag.description:
            node.doc = tag.description
            node.doc_position = tag.position

        if ANN_SKIP in annotations:
            node.skip = True

        if annotations:
            attributes_annotation = annotations.get(ANN_ATTRIBUTES)
            if attributes_annotation is not None:
                for key, value in attributes_annotation.items():
                    if value:
                        node.attributes[key] = value

    def _apply_annotations_annotated(self, node, block):
        if block is None:
            return

        if block.description:
            node.doc = block.description
            node.doc_position = block.position

        since_tag = block.tags.get(TAG_SINCE)
        if since_tag is not None:
            if since_tag.value:
                node.version = since_tag.value
            if since_tag.description:
                node.version_doc = since_tag.description

        deprecated_tag = block.tags.get(TAG_DEPRECATED)
        if deprecated_tag is not None:
            if deprecated_tag.value:
                node.deprecated = deprecated_tag.value
            if deprecated_tag.description:
                node.deprecated_doc = deprecated_tag.description

        stability_tag = block.tags.get(TAG_STABILITY)
        if stability_tag is not None:
            if stability_tag.value:
                node.stability = stability_tag.value
            if stability_tag.description:
                node.stability_doc = stability_tag.description

        attributes_annotation = block.annotations.get(ANN_ATTRIBUTES)
        if attributes_annotation is not None:
            for key, value in attributes_annotation.items():
                if value:
                    node.attributes[key] = value

        if ANN_SKIP in block.annotations:
            node.skip = True

        if ANN_FOREIGN in block.annotations:
            node.foreign = True

        if ANN_CONSTRUCTOR in block.annotations and isinstance(node, ast.Function):
            node.is_constructor = True

        if ANN_METHOD in block.annotations:
            node.is_method = True

        set_property = block.annotations.get(ANN_SET_PROPERTY)
        if set_property is not None and isinstance(node, ast.Function):
            node.set_property = set_property[0]

        get_property = block.annotations.get(ANN_GET_PROPERTY)
        if get_property is not None and isinstance(node, ast.Function):
            node.get_property = get_property[0]

    def _apply_annotations_alias(self, node, chain):
        block = self._get_block(node)
        self._apply_annotations_annotated(node, block)

    def _apply_annotations_param(self, parent, param, tag):
        annotations = tag.annotations if tag else {}

        if isinstance(parent, (ast.Function, ast.VFunction)):
            scope_annotation = annotations.get(ANN_SCOPE)
            if scope_annotation and len(scope_annotation) == 1:
                param.scope = scope_annotation[0]
                param.transfer = ast.PARAM_TRANSFER_NONE

            destroy_annotation = annotations.get(ANN_DESTROY)
            if destroy_annotation:
                param.destroy_name = self._get_validate_parameter_name(parent,
                                                                       destroy_annotation[0],
                                                                       param)
                if param.destroy_name is not None:
                    param.scope = ast.PARAM_SCOPE_NOTIFIED
                    destroy_param = parent.get_parameter(param.destroy_name)
                    # This is technically bogus; we're setting the scope on the destroy
                    # itself.  But this helps avoid tripping a warning from finaltransformer,
                    # since we don't have a way right now to flag this callback a destroy.
                    destroy_param.scope = ast.PARAM_SCOPE_NOTIFIED

            closure_annotation = annotations.get(ANN_CLOSURE)
            if closure_annotation and len(closure_annotation) == 1:
                param.closure_name = self._get_validate_parameter_name(parent,
                                                                   closure_annotation[0],
                                                                   param)
        elif isinstance(parent, ast.Callback):
            if ANN_CLOSURE in annotations:
                # For callbacks, (closure) appears without an
                # argument, and tags a parameter that is a closure. We
                # represent it (weirdly) in the gir and typelib by
                # setting param.closure_name to itself.
                param.closure_name = param.argname

        self._apply_annotations_param_ret_common(parent, param, tag)

    def _apply_annotations_return(self, parent, return_, block):
        if block:
            tag = block.tags.get(TAG_RETURNS)
        else:
            tag = None

        if tag is not None and return_.type == ast.TYPE_NONE:
            message.warn('%s: invalid return annotation' % (block.name,),
                         tag.position)
            tag = None

        self._apply_annotations_param_ret_common(parent, return_, tag)

    def _apply_annotations_params(self, parent, params, block):
        declparams = set([])
        if parent.instance_parameter:
            if block:
                doc_param = block.params.get(parent.instance_parameter.argname)
            else:
                doc_param = None
            self._apply_annotations_param(parent, parent.instance_parameter, doc_param)
            declparams.add(parent.instance_parameter.argname)

        for param in params:
            if block:
                doc_param = block.params.get(param.argname)
            else:
                doc_param = None
            self._apply_annotations_param(parent, param, doc_param)
            declparams.add(param.argname)

        if not block:
            return
        docparams = set(block.params)

        unknown = docparams - declparams
        unused = declparams - docparams

        for doc_name in unknown:
            if len(unused) == 0:
                text = ''
            elif len(unused) == 1:
                (param, ) = unused
                text = ", should be '%s'" % (param, )
            else:
                text = ", should be one of %s" % \
                       (', '.join("'%s'" % p for p in sorted(unused)), )

            param = block.params.get(doc_name)
            message.warn("%s: unknown parameter '%s' in documentation "
                         "comment%s" % (block.name, doc_name, text),
                param.position)

    def _apply_annotations_callable(self, node, chain, block):
        if block is not None:
            finish_func = block.annotations.get(ANN_FINISH_FUNC)
            if finish_func is not None and isinstance(node, ast.Callable):
                node.finish_func = finish_func[0]

            sync_func = block.annotations.get(ANN_SYNC_FUNC)
            if sync_func is not None and isinstance(node, ast.Callable):
                node.sync_func = sync_func[0]

            async_func = block.annotations.get(ANN_ASYNC_FUNC)
            if async_func is not None and isinstance(node, ast.Callable):
                node.async_func = async_func[0]
        self._apply_annotations_annotated(node, block)
        self._apply_annotations_params(node, node.parameters, block)
        self._apply_annotations_return(node, node.retval, block)

    def _apply_annotations_field(self, parent, parent_block, field):
        block = self._blocks.get('%s.%s' % (self._get_annotation_name(parent), field.name))

        # Prioritize block level documentation
        if block:
            self._apply_annotations_annotated(field, block)
            annotations = block.annotations
        elif not parent_block:
            return
        else:
            tag = parent_block.params.get(field.name)
            if not tag:
                return
            annotations = tag.annotations
            field.doc = tag.description
            field.doc_position = tag.position

        type_annotation = annotations.get(ANN_TYPE)
        if type_annotation:
            field.type = self._transformer.create_type_from_user_string(type_annotation[0])
        try:
            self._adjust_container_type(parent, field, annotations)
        except AttributeError as ex:
            print(ex)

    def _apply_annotations_property(self, parent, prop):
        prefix = self._get_annotation_name(parent)
        block = self._blocks.get('%s:%s' % (prefix, prop.name))
        self._apply_annotations_annotated(prop, block)
        if not block:
            return
        transfer_annotation = block.annotations.get(ANN_TRANSFER)
        if transfer_annotation is not None:
            transfer = transfer_annotation[0]
            if transfer == OPT_TRANSFER_FLOATING:
                transfer = OPT_TRANSFER_NONE
            prop.transfer = transfer
        else:
            prop.transfer = self._get_transfer_default(parent, prop)
        type_annotation = block.annotations.get(ANN_TYPE)
        if type_annotation:
            prop.type = self._resolve_toplevel(type_annotation[0], prop.type, prop, parent)
        setter = block.annotations.get(ANN_SETTER)
        if setter:
            prop.setter = setter[0]
        getter = block.annotations.get(ANN_GETTER)
        if getter:
            prop.getter = getter[0]
        default_value = block.annotations.get(ANN_DEFAULT_VALUE)
        if default_value:
            prop.default_value = default_value[0]

    def _apply_annotations_signal(self, parent, signal):
        names = []
        prefix = self._get_annotation_name(parent)
        block = self._blocks.get('%s::%s' % (prefix, signal.name))

        if block:
            self._apply_annotations_annotated(signal, block)

            emitter = block.annotations.get(ANN_EMITTER)
            if emitter:
                signal.emitter = emitter[0]

            # We're only attempting to name the signal parameters if
            # the number of parameters (@foo) is the same or greater
            # than the number of signal parameters
            if len(block.params) > len(signal.parameters):
                names = [(k, v) for k, v in block.params.items()]
                # Resolve real parameter names early, so that in later
                # phase we can refer to them while resolving annotations.
                for i, param in enumerate(signal.parameters):
                    param.argname, tag = names[i + 1]
            elif len(signal.parameters) != 0:
                # Only warn about missing params if there are actually parameters
                # besides implicit self.
                message.warn("incorrect number of parameters in comment block, "
                             "parameter annotations will be ignored.", block.position)

        for i, param in enumerate(signal.parameters):
            if names:
                name, tag = names[i + 1]
                if tag:
                    type_annotation = tag.annotations.get(ANN_TYPE)
                    if type_annotation:
                        param.type = self._resolve_toplevel(type_annotation[0], param.type,
                                                            param, parent)
            else:
                tag = None
            self._apply_annotations_param(signal, param, tag)
        self._apply_annotations_return(signal, signal.retval, block)

    def _apply_annotations_constant(self, node):
        block = self._get_block(node)
        if block is None:
            return

        self._apply_annotations_annotated(node, block)

        value_annotation = block.annotations.get(ANN_VALUE)
        if value_annotation:
            node.value = value_annotation[0]

    def _apply_annotations_enum_members(self, node, parent_block):
        for m in node.members:
            block = self._blocks.get(m.symbol)
            # Prioritize block-level documentation
            if block:
                self._apply_annotations_annotated(m, block)
            elif parent_block:
                param = parent_block.params.get(m.symbol)

                if not param:
                    continue

                if param.description:
                    m.doc = param.description
                    m.doc_position = param.position

    def _pass_read_annotations2(self, node, chain):
        if isinstance(node, ast.Function):
            block = self._blocks.get(node.symbol)

            self._apply_annotation_rename_to(node, chain, block)

            # Handle virtual invokers
            parent = chain[-1] if chain else None
            if (block and parent):
                virtual_annotation = block.annotations.get(ANN_VFUNC)
                if virtual_annotation:
                    invoker_name = virtual_annotation[0]
                    matched = False
                    for vfunc in parent.virtual_methods:
                        if vfunc.name == invoker_name:
                            matched = True
                            vfunc.invoker = node.name
                            # Also merge in annotations
                            self._apply_annotations_callable(vfunc, [parent], block)
                            break
                    if not matched:
                        message.warn_node(node,
                            "Virtual slot '%s' not found for '%s' annotation" % (invoker_name,
                                                                             ANN_VFUNC))
        return True

    def _resolve_and_filter_type_list(self, typelist):
        """Given a list of Type instances, return a new list of types with
the ones that failed to resolve removed."""
        # Create a copy we'll modify
        new_typelist = list(typelist)
        for typeval in typelist:
            resolved = self._transformer.resolve_type(typeval)
            if not resolved:
                new_typelist.remove(typeval)
        return new_typelist

    def _pass_type_resolution(self, node, chain):
        if isinstance(node, ast.Alias):
            self._transformer.resolve_type(node.target)
        if isinstance(node, ast.Callable):
            for parameter in node.parameters:
                self._transformer.resolve_type(parameter.type)
            self._transformer.resolve_type(node.retval.type)
        if isinstance(node, ast.Constant):
            self._transformer.resolve_type(node.value_type)
        if isinstance(node, (ast.Class, ast.Interface, ast.Record, ast.Union)):
            for field in node.fields:
                if field.anonymous_node:
                    pass
                else:
                    self._transformer.resolve_type(field.type)
        if isinstance(node, (ast.Class, ast.Interface)):
            for parent in node.parent_chain:
                try:
                    self._transformer.resolve_type(parent)
                except ValueError:
                    continue
                target = self._transformer.lookup_typenode(parent)
                if target:
                    node.parent_type = parent
                    break
            else:
                if isinstance(node, ast.Interface):
                    node.parent_type = ast.Type(target_giname='GObject.Object')
            for prop in node.properties:
                self._transformer.resolve_type(prop.type)
            for sig in node.signals:
                for param in sig.parameters:
                    self._transformer.resolve_type(param.type)
        if isinstance(node, ast.Class):
            node.interfaces = self._resolve_and_filter_type_list(node.interfaces)
        if isinstance(node, ast.Interface):
            node.prerequisites = self._resolve_and_filter_type_list(node.prerequisites)
        return True

    def _pair_quarks_with_enums(self):
        # self._uscore_type_names is an authoritative mapping of types
        # to underscored versions, since it is based on get_type() methods;
        # but only covers enums that are registered as GObject enums.
        # Create a fallback mapping based on all known enums in this module.
        uscore_enums = {}
        for enum in self._namespace.values():
            if not isinstance(enum, ast.Enum):
                continue
            uscored = to_underscores_noprefix(enum.name).lower()
            uscore_enums[uscored] = enum
            uscore_enums[enum.name] = enum

        for node in self._namespace.values():
            if not isinstance(node, ast.ErrorQuarkFunction):
                continue
            full = node.symbol[:-len('_quark')]
            ns, short = self._transformer.split_csymbol(node.symbol)
            short = short[:-len('_quark')]
            if full == "g_io_error":
                # Special case; GIOError was already taken forcing GIOErrorEnum
                assert self._namespace.name == 'Gio'
                enum = self._namespace.get('IOErrorEnum')
            else:
                enum = self._uscore_type_names.get(short)
                if enum is None:
                    enum = uscore_enums.get(short)
            if enum is not None:
                enum.error_domain = node.error_domain
            else:
                message.warn_node(node,
                    """%s: Couldn't find corresponding enumeration""" % (node.symbol, ))

    def _split_uscored_by_type(self, uscored):
        """'uscored' should be an un-prefixed uscore string.  This
function searches through the namespace for the longest type which
prefixes uscored, and returns (type, suffix).  Example, assuming
namespace Gtk, type is TextBuffer:

_split_uscored_by_type(text_buffer_try_new) -> (ast.Class(TextBuffer), 'try_new')"""
        node = None
        count = 0
        prev_split_count = -1
        while True:
            components = uscored.rsplit('_', count)
            if len(components) == prev_split_count:
                return None
            prev_split_count = len(components)
            type_string = components[0]
            node = self._uscore_type_names.get(type_string)
            if node:
                return (node, '_'.join(components[1:]))
            count += 1

    def _pair_function(self, func):
        """Check to see whether a toplevel function should be a
method or constructor of some type."""

        # Ignore internal symbols and type metadata functions
        if func.symbol.startswith('_') or func.is_type_meta_function():
            return

        (ns, subsymbol) = self._transformer.split_csymbol(func.symbol)
        assert ns == self._namespace
        if self._is_constructor(func, subsymbol):
            self._set_up_constructor(func, subsymbol)
            return
        elif self._is_method(func, subsymbol):
            self._setup_method(func, subsymbol)
            return
        elif self._pair_static_method(func, subsymbol):
            return

    def _uscored_identifier_for_type(self, typeval):
        """Given a Type(target_giname='Foo.BarBaz'), return 'bar_baz'."""
        name = typeval.get_giname()
        return to_underscores_noprefix(name).lower()

    def _is_method(self, func, subsymbol):
        if not func.parameters:
            if func.is_method:
                message.warn_node(func,
                    '%s: Methods must have parameters' % (func.symbol, ))
            return False
        first = func.parameters[0]
        target = self._transformer.lookup_typenode(first.type)
        if not isinstance(target, (ast.Class, ast.Interface,
                                   ast.Record, ast.Union,
                                   ast.Boxed)):
            if func.is_method:
                message.warn_node(func,
                    '%s: Methods must have a pointer as their first '
                    'parameter' % (func.symbol, ))
            return False
        if target.namespace != self._namespace:
            if func.is_method:
                message.warn_node(func,
                    '%s: Methods must belong to the same namespace as the '
                    'class they belong to' % (func.symbol, ))
            return False
        if first.direction == ast.PARAM_DIRECTION_OUT:
            if func.is_method:
                message.warn_node(func,
                    '%s: The first argument of methods cannot be an '
                    'out-argument' % (func.symbol, ))
            return False

        # A quick hack here...in the future we should catch C signature/GI signature
        # mismatches in a general way in finaltransformer
        if first.type.ctype is not None and first.type.ctype.count('*') > 1:
            return False

        if not func.is_method:
            uscored_prefix = self._get_uscored_prefix(func, subsymbol)
            if not subsymbol.startswith(uscored_prefix):
                return False

        return True

    def _setup_method(self, func, subsymbol):
        uscored_prefix = self._get_uscored_prefix(func, subsymbol)
        target = self._transformer.lookup_typenode(func.parameters[0].type)

        if not func.is_method and not subsymbol.startswith(uscored_prefix + '_'):
            # Uh oh! This function starts with uscored_prefix, but not
            # uscored_prefix + '_', so if we split, we're splitting on something
            # which is not _
            # Examples of this are g_resources_register() (splits as
            # g_resource + _register) and gdk_events_get_angle() (splits as
            # gdk_event + _get_angle).
            # As the C name suggests, these are not methods, but for backward
            # compatibility reasons we need to create a method with the old
            # name, and a moved-to annotation pointing to the new variant.

            newfunc = func.clone()
            newfunc.moved_to = func.name
            newfunc.instance_parameter = newfunc.parameters.pop(0)
            subsym_idx = func.symbol.find(subsymbol)
            newfunc.name = func.symbol[(subsym_idx + len(uscored_prefix) + 1):]
            newfunc.is_method = True

            target.methods.append(newfunc)
        else:
            func.instance_parameter = func.parameters.pop(0)
            self._namespace.float(func)

            if not func.is_method:
                subsym_idx = func.symbol.find(subsymbol)
                func.name = func.symbol[(subsym_idx + len(uscored_prefix) + 1):]
                func.is_method = True

            target.methods.append(func)

    def _get_uscored_prefix(self, func, subsymbol):
        # Here we check both the c_symbol_prefix and (if that fails),
        # attempt to do a default uscoring of the type.  The reason we
        # look at a default underscore transformation is for
        # gdk_window_object_get_type(), which says to us that the
        # prefix is "gdk_window_object", when really it's just
        # "gdk_window".  Possibly need an annotation to override this.
        prefix_matches = False
        uscored_prefix = None
        first_arg = func.parameters[0]
        target = self._transformer.lookup_typenode(first_arg.type)
        if hasattr(target, 'c_symbol_prefix') and target.c_symbol_prefix is not None:
            prefix_matches = subsymbol.startswith(target.c_symbol_prefix)
            if prefix_matches:
                uscored_prefix = target.c_symbol_prefix
        if not prefix_matches:
            uscored_prefix = self._uscored_identifier_for_type(first_arg.type)

        return uscored_prefix

    def _pair_static_method(self, func, subsymbol):
        split = self._split_uscored_by_type(subsymbol)
        if split is None:
            return False
        (node, funcname) = split
        if funcname == '':
            return False

        if isinstance(node, ast.Class):
            self._namespace.float(func)
            func.name = funcname
            node.static_methods.append(func)
            return True
        elif isinstance(node, (ast.Interface, ast.Record, ast.Union,
                               ast.Boxed, ast.Enum, ast.Bitfield)):
            # prior to the introduction of this part of the code, only
            # ast.Class could have static methods.  so for backwards
            # compatibility, instead of removing the func from the namespace,
            # leave it there and get a copy instead.  modify the copy and push
            # it onto static_methods.  we need to copy the parameters list
            # separately, because in the third pass functions are flagged as
            # 'throws' depending on the presence of a GError parameter which is
            # then removed from the parameters list.  without the explicit
            # copy, only one of the two functions would thus get flagged as
            # 'throws'.  clone() does this for us.
            new_func = func.clone()
            new_func.name = funcname
            node.static_methods.append(new_func)
            # flag the func as a backwards-comptability kludge (thus it will
            # get pruned in the introspectable pass if introspectable=0).
            func.moved_to = node.name + '.' + new_func.name
            return True

        return False

    def _set_up_constructor(self, func, subsymbol):
        self._namespace.float(func)

        func.name = self._get_constructor_name(func, subsymbol)

        origin_node = self._get_constructor_class(func, subsymbol)
        origin_node.constructors.append(func)

        func.is_constructor = True

        # Constructors have default return semantics
        if not func.retval.transfer:
            func.retval.transfer = self._get_transfer_default_return(func,
                    func.retval)

    def _get_constructor_class(self, func, subsymbol):
        origin_node = None
        split = self._split_uscored_by_type(subsymbol)
        if split is None:
            if func.is_constructor:
                origin_node = self._transformer.lookup_typenode(func.retval.type)
        else:
            origin_node, _ = split

        return origin_node

    def _get_constructor_name(self, func, subsymbol):
        prefix_matches = False
        uscored_prefix = None
        target = self._transformer.lookup_typenode(func.retval.type)
        if hasattr(target, 'c_symbol_prefix') and target.c_symbol_prefix is not None:
            prefix_matches = subsymbol.startswith(target.c_symbol_prefix)
            if prefix_matches:
                uscored_prefix = target.c_symbol_prefix
        if not prefix_matches:
            uscored_prefix = self._uscored_identifier_for_type(func.retval.type)
        split = self._split_uscored_by_type(subsymbol)
        if split is None:
            if func.is_constructor:
                if uscored_prefix in func.symbol:
                    subsym_idx = func.symbol.find(subsymbol)
                    func.name = func.symbol[(subsym_idx + len(uscored_prefix) + 1):]
                name = func.name
        else:
            _, name = split

        return name

    def _guess_constructor_by_name(self, symbol):
        # Normal constructors, gtk_button_new etc
        if symbol.endswith('_new'):
            return True
        # Alternative constructor, gtk_button_new_with_label
        if '_new_' in symbol:
            return True
        # gtk_list_store_newv,gtk_tree_store_newv etc
        if symbol.endswith('_newv'):
            return True
        return False

    def _is_constructor(self, func, subsymbol):
        # func.is_constructor will be True if we have a (constructor) annotation
        if not func.is_constructor:
            if not self._guess_constructor_by_name(func.symbol):
                return False
        target = self._transformer.lookup_typenode(func.retval.type)
        if not (isinstance(target, ast.Class)
                or (isinstance(target, (ast.Record, ast.Union, ast.Boxed))
                    and (target.get_type is not None or target.foreign))):
            if func.is_constructor:
                message.warn_node(func,
                    '%s: Constructors must return an instance of their class'
                    % (func.symbol, ))
            return False

        origin_node = self._get_constructor_class(func, subsymbol)
        if origin_node is None:
            if func.is_constructor:
                message.warn_node(
                    func,
                    "Can't find matching type for constructor; symbol='%s'" % (func.symbol, ))
            return False

        # Some sanity checks; only objects and boxeds can have ctors
        if not (isinstance(origin_node, ast.Class)
                or (isinstance(origin_node, (ast.Record, ast.Union, ast.Boxed))
                    and (origin_node.get_type is not None or origin_node.foreign))):
            return False
        # Verify the namespace - don't want to append to foreign namespaces!
        if origin_node.namespace != self._namespace:
            if func.is_constructor:
                message.warn_node(func,
                    '%s: Constructors must belong to the same namespace as the '
                    'class they belong to' % (func.symbol, ))
            return False
        # If it takes the object as a first arg, guess it's not a constructor
        if not func.is_constructor and len(func.parameters) > 0:
            first_arg = self._transformer.lookup_typenode(func.parameters[0].type)
            if (first_arg is not None) and first_arg.gi_name == origin_node.gi_name:
                return False

        if isinstance(target, ast.Class):
            parent = origin_node
            while parent and (not parent.gi_name == 'GObject.Object'):
                if parent == target:
                    break
                if parent.parent_type:
                    parent = self._transformer.lookup_typenode(parent.parent_type)
                else:
                    parent = None
                if parent is None:
                    message.warn_node(func,
                                      "Return value is not superclass for constructor; "
                                      "symbol='%s' constructed='%s' return='%s'" %
                                      (func.symbol,
                                       str(origin_node.create_type()),
                                       str(func.retval.type)))
                    return False
        else:
            if origin_node != target:
                message.warn_node(func,
                                  "Constructor return type mismatch symbol='%s' "
                                  "constructed='%s' return='%s'" %
                                  (func.symbol,
                                   str(origin_node.create_type()),
                                   str(func.retval.type)))
                return False

        return True

    def _pair_class_virtuals(self, node):
        """Look for virtual methods from the class structure."""
        if not node.glib_type_struct:
            # https://bugzilla.gnome.org/show_bug.cgi?id=629080
            # message.warn_node(node,
            #    "Failed to find class structure for '%s'" % (node.name, ))
            return

        node_type = node.create_type()
        class_struct = self._transformer.lookup_typenode(node.glib_type_struct)

        # Object class fields are assumed to be read-only
        # (see also _introspect_object and transformer.py)
        for field in class_struct.fields:
            if isinstance(field, ast.Field):
                field.writable = False

        for field in class_struct.fields:
            callback = None

            if isinstance(field.anonymous_node, ast.Callback):
                callback = field.anonymous_node
            elif field.type is not None:
                callback = self._transformer.lookup_typenode(field.type)
                if not isinstance(callback, ast.Callback):
                    continue
            else:
                continue

            # Check the first parameter is the object
            if len(callback.parameters) == 0:
                continue
            firstparam_type = callback.parameters[0].type
            if firstparam_type != node_type:
                continue
            vfunc = ast.VFunction.from_callback(field.name, callback)
            vfunc.instance_parameter = callback.parameters[0]
            vfunc.inherit_file_positions(callback)

            prefix = self._get_annotation_name(class_struct)
            # Prefer full docblocks, but fall back to the field description
            # if there isn't a full one, to avoid an undocumented symbol
            block = self._blocks.get('%s::%s' % (prefix, vfunc.name))
            if block is None:
                vfunc.doc = field.doc
                vfunc.doc_position = field.doc_position
            self._apply_annotations_callable(vfunc, [node], block)
            node.virtual_methods.append(vfunc)

        # Take the set of virtual methods we found, and try
        # to pair up with any matching methods using the
        # name+signature.
        for vfunc in node.virtual_methods:
            for method in node.methods:
                if method.name != vfunc.name:
                    continue
                if method.retval.type != vfunc.retval.type:
                    continue
                if len(method.parameters) != len(vfunc.parameters):
                    continue
                for i in range(len(method.parameters)):
                    m_type = method.parameters[i].type
                    v_type = vfunc.parameters[i].type
                    if m_type != v_type:
                        continue
                vfunc.invoker = method.name
                # Apply any annotations we have from the invoker to
                # the vfunc
                block = self._blocks.get(method.symbol)
                self._apply_annotations_callable(vfunc, [], block)
                break

    def _pair_property_accessors(self, node):
        """Look for accessor methods for class properties"""
        for prop in node.properties:
            if not prop.introspectable:
                continue
            if prop.setter is None:
                normalized_name = prop.name.replace('-', '_')
                if prop.writable and not prop.construct_only:
                    setter = 'set_' + normalized_name
                else:
                    setter = None
            else:
                setter = prop.setter
            if prop.getter is None:
                if prop.readable:
                    getter = ['get_' + normalized_name]
                    # Heuristic: read-only properties can have getters that are
                    # just the property name, like: gtk_widget_has_focus()
                    if not prop.writable and prop.type.is_equiv(ast.TYPE_BOOLEAN):
                        getter.append(normalized_name)
                else:
                    getter = []
            else:
                getter = [prop.getter]
            for method in node.methods:
                if not method.introspectable:
                    continue
                if setter is not None and method.name == setter:
                    if method.set_property is None:
                        method.set_property = prop.name
                    elif method.set_property != prop.name:
                        message.warn_node(method,
                                          "Setter method '%s' for property '%s' has a "
                                          "mismatched '(set-property %s)' annotation" %
                                          (method.symbol, prop.name, method.set_property))
                        method.set_property = prop.name
                    prop.setter = method.name
                    continue
                if getter is not [] and method.name in getter:
                    if method.get_property is None:
                        method.get_property = prop.name
                    elif method.get_property != prop.name:
                        message.warn_node(method,
                                          "Getter method '%s' for property '%s' has a "
                                          "mismatched '(get-property %s)' annotation" %
                                          (method.symbol, prop.name, method.get_property))
                        method.get_property = prop.name
                    prop.getter = method.name
                    continue

    def _pass3(self, node, chain):
        """Pass 3 is after we've loaded GType data and performed type
        closure."""
        if isinstance(node, ast.Callable):
            self._pass3_callable_callbacks(node)
            self._pass3_callable_throws(node)
            self._pass3_callable_async_finish(node)
            self._pass3_callable_async_sync(node)
        elif isinstance(node, (ast.Class, ast.Interface)):
            self._pass3_class_async_finish(node)
            self._pass3_class_async_sync(node)
        return True

    def _pass3_class_async_finish(self, node):
        self._match_class_async_methods(node.methods)
        self._match_class_async_methods(node.virtual_methods)
        self._match_class_async_methods(node.static_methods + node.constructors)

    def _pass3_class_async_sync(self, node):
        self._match_class_sync_methods(node.methods)
        self._match_class_sync_methods(node.virtual_methods)
        self._match_class_sync_methods(node.static_methods)

    def _pass3_callable_async_finish(self, node):
        if node.finish_func is not None:
            return
        for param in node.parameters:
            if param.type.ctype is None or param.type.ctype not in ('GAsyncReadyCallback'):
                continue
            func_name = node.name + '_finish'
            if (node.name.endswith('_async')):
                func_name = node.name[:-6] + '_finish'
            if node.parent is None:
                continue
            if ('GAsyncResult') in node.parent.ctypes:
                if func_name in node.parent:
                    node.finish_func = func_name
                    break
                else:
                    message.warn_node(node,
                    "Couldn't find '%s' for the corresponding async function: '%s'"
                    % (func_name, node.name))

    def _pass3_callable_async_sync(self, node):
        if node.sync_func is not None or node.finish_func is None:
            return
        if (node.name.endswith('_async')):
            sync_name = node.name[:-6]
        else:
            sync_name = node.name + '_sync'
        if node.parent is None:
            return
        finish_func = node.parent.get(node.finish_func)
        if finish_func is None:
            return
        candidate_method = node.parent.get(sync_name)
        if candidate_method is None:
            return
        if finish_func.retval.type.ctype != candidate_method.retval.type.ctype:
            return
        param_matched = True
        for param in candidate_method.parameters:
            if param.direction == ast.PARAM_DIRECTION_IN:
                for candidate_param in node.parameters:
                    if (candidate_param.direction == ast.PARAM_DIRECTION_IN and
                            candidate_param.name == param.name):
                        break
                else:
                    param_matched = False
                    break
            elif param.direction == ast.PARAM_DIRECTION_OUT:
                for candidate_param in finish_func.parameters:
                    if (candidate_param.direction == ast.PARAM_DIRECTION_OUT and
                            candidate_param.name == param.name):
                        break
                else:
                    param_matched = False
                    break
        if param_matched:
            node.sync_func = candidate_method.name
            candidate_method.async_func = node.name
            return

    def _match_class_async_methods(self, methods):
        for method in methods:
            if method.finish_func is not None:
                continue
            func_name = method.name + '_finish'
            if method.name.endswith('_async'):
                func_name = method.name[:-6] + '_finish'
            found_async_callback_param = False
            for params in method.parameters:
                if (params.type.ctype == 'GAsyncReadyCallback'):
                    found_async_callback_param = True

            if not found_async_callback_param:
                continue

            found_finish_method = False
            for candidate_method in methods:
                if found_finish_method:
                    break
                if candidate_method.name != func_name:
                    continue
                for candidate_param in candidate_method.parameters:
                    if candidate_param.type.ctype == 'GAsyncResult*':
                        method.finish_func = candidate_method.name
                        found_finish_method = True
                        break
            if not found_finish_method:
                message.warn_node(method,
                "Couldn't find '%s' for the corresponding async function: '%s'"
                % (func_name, method.name))

    def _match_class_sync_methods(self, methods):
        for method in methods:
            if method.sync_func is not None or method.finish_func is None:
                continue
            if method.name.endswith('_async'):
                sync_name = method.name[:-6]
            else:
                sync_name = method.name + '_sync'
            param_matched = True
            for finish_candidate_method in methods:
                type_name = finish_candidate_method.retval.type.ctype
                if finish_candidate_method.name == method.finish_func:
                    type_name = finish_candidate_method.retval.type.ctype
                    break
            for candidate_method in methods:
                if candidate_method.name != sync_name:
                    continue
                if type_name != candidate_method.retval.type.ctype:
                    param_matched = False
                    break
                for param in candidate_method.parameters:
                    if param.direction == ast.PARAM_DIRECTION_IN:
                        for candidate_param in method.parameters:
                            if (candidate_param.direction == ast.PARAM_DIRECTION_IN and
                                    candidate_param.name == param.name):
                                break
                        else:
                            param_matched = False
                            break
                    elif param.direction == ast.PARAM_DIRECTION_OUT:
                        for candidate_param in finish_candidate_method.parameters:
                            if (candidate_param.direction == ast.PARAM_DIRECTION_OUT and
                                    candidate_param.name == param.name):
                                break
                        else:
                            param_matched = False
                            break
                if param_matched:
                    method.sync_func = candidate_method.name
                    candidate_method.async_func = method.name
                    break

    def _pass3_callable_callbacks(self, node):
        """Check to see if we have anything that looks like a
        callback+user_data+GDestroyNotify set."""
        params = node.parameters

        # First, do defaults for well-known callback types
        for param in params:
            argnode = self._transformer.lookup_typenode(param.type)
            if isinstance(argnode, ast.Callback):
                if param.type.target_giname in ('Gio.AsyncReadyCallback',
                                                'GLib.DestroyNotify'):
                    param.scope = ast.PARAM_SCOPE_ASYNC
                    param.transfer = ast.PARAM_TRANSFER_NONE

        callback_param = None
        for param in params:
            argnode = self._transformer.lookup_typenode(param.type)
            is_destroynotify = False
            if isinstance(argnode, ast.Callback):
                if param.type.target_giname == 'GLib.DestroyNotify':
                    is_destroynotify = True
                else:
                    callback_param = param
                    continue
            if callback_param is None:
                continue
            if is_destroynotify:
                callback_param.destroy_name = param.argname
                callback_param.scope = ast.PARAM_SCOPE_NOTIFIED
                callback_param.transfer = ast.PARAM_TRANSFER_NONE
            elif (param.type.is_equiv(ast.TYPE_ANY) and
                  param.argname is not None and
                  param.argname.endswith('data')):
                callback_param.closure_name = param.argname

        for param in params:
            # By convention, closure user_data parameters are always nullable.
            if param.closure_name is not None:
                idx = node.get_parameter_index(param.closure_name)
                assert idx >= 0
                closure_param = params[idx]
                if not closure_param.not_nullable:
                    closure_param.nullable = True

    def _pass3_callable_throws(self, node):
        """Check to see if we have anything that looks like a
        callback+user_data+GDestroyNotify set."""
        if not node.parameters:
            return
        last_param = node.parameters[-1]
        # Checking type.name=='GLib.Error' generates false positives
        # on methods that take a 'GError *'
        if last_param.type.ctype == 'GError**':
            node.parameters.pop()
            node.throws = True
