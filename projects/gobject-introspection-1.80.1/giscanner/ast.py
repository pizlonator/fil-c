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

import copy
import operator
from itertools import chain
from collections import OrderedDict

from .sourcescanner import CTYPE_TYPEDEF, CSYMBOL_TYPE_TYPEDEF
from .message import Position, warn
from .utils import to_underscores


class Type(object):
    """
    A Type can be either:
    * A reference to a node (target_giname)
    * A reference to a "fundamental" type like 'utf8'
    * A "foreign" type - this can be any string."
    If none are specified, then it's in an "unresolved" state.  An
    unresolved type can have two data sources; a "ctype" which comes
    from a C type string, or a gtype_name (from g_type_name()).
    """

    def __init__(self,
                 ctype=None,
                 gtype_name=None,
                 target_fundamental=None,
                 target_giname=None,
                 target_foreign=None,
                 _target_unknown=False,
                 is_const=False,
                 origin_symbol=None,
                 complete_ctype=None):
        self.ctype = ctype
        self.gtype_name = gtype_name
        self.origin_symbol = origin_symbol
        if _target_unknown:
            assert isinstance(self, TypeUnknown)
        elif target_fundamental:
            assert target_giname is None
            assert target_foreign is None
        elif target_giname:
            assert '.' in target_giname
            assert target_fundamental is None
            assert target_foreign is None
        elif target_foreign:
            assert ctype is not None
            assert target_giname is None
            assert target_fundamental is None
        else:
            assert (ctype is not None) or (gtype_name is not None)
        self.target_fundamental = target_fundamental
        self.target_giname = target_giname
        self.target_foreign = target_foreign
        self.is_const = is_const
        self.complete_ctype = complete_ctype

    @property
    def resolved(self):
        return (self.target_fundamental or
                self.target_giname or
                self.target_foreign)

    @property
    def unresolved_string(self):
        if self.ctype:
            return self.ctype
        elif self.gtype_name:
            return self.gtype_name
        elif self.target_giname:
            return self.target_giname
        else:
            assert False

    @classmethod
    def create_from_gtype_name(cls, gtype_name):
        """Parse a GType name (as from g_type_name()), and return a
Type instance.  Note that this function performs namespace lookup,
in contrast to the other create_type() functions."""
        # First, is it a fundamental?
        fundamental = type_names.get(gtype_name)
        if fundamental is not None:
            return cls(target_fundamental=fundamental.target_fundamental,
                       ctype=fundamental.ctype)
        if gtype_name == 'GHashTable':
            return Map(TYPE_ANY, TYPE_ANY, gtype_name=gtype_name)
        elif gtype_name == 'GByteArray':
            return Array('GLib.ByteArray', TYPE_UINT8, gtype_name=gtype_name)
        elif gtype_name in ('GArray', 'GPtrArray'):
            return Array('GLib.' + gtype_name[1:], TYPE_ANY,
                         gtype_name=gtype_name)
        elif gtype_name == 'GStrv':
            bare_utf8 = TYPE_STRING.clone()
            bare_utf8.ctype = None
            return Array(None, bare_utf8, ctype=None, gtype_name=gtype_name,
                         is_const=False)

        return cls(gtype_name=gtype_name)

    def get_giname(self):
        assert self.target_giname is not None
        return self.target_giname.split('.')[1]

    def _compare(self, other, op):
        if self.target_fundamental:
            return op(self.target_fundamental, other.target_fundamental)
        elif self.target_giname:
            return op(self.target_giname, other.target_giname)
        elif self.target_foreign:
            return op(self.target_foreign, other.target_foreign)
        else:
            return op(self.ctype, other.ctype)

    def __lt__(self, other):
        return self._compare(other, operator.lt)

    def __gt__(self, other):
        return self._compare(other, operator.gt)

    def __ge__(self, other):
        return self._compare(other, operator.ge)

    def __le__(self, other):
        return self._compare(other, operator.le)

    def __eq__(self, other):
        return self._compare(other, operator.eq)

    def __ne__(self, other):
        return self._compare(other, operator.ne)

    def __hash__(self):
        return hash((self.target_fundamental, self.target_giname,
                     self.target_foreign, self.ctype))

    def is_equiv(self, typeval):
        """Return True if the specified types are compatible at
        an introspection level, disregarding their C types.
        A sequence may be given for typeval, in which case
        this function returns True if the type is compatible with
        any."""
        if isinstance(typeval, (list, tuple)):
            for val in typeval:
                if self.is_equiv(val):
                    return True
            return False
        return self == typeval

    def clone(self):
        return Type(target_fundamental=self.target_fundamental,
                    target_giname=self.target_giname,
                    target_foreign=self.target_foreign,
                    ctype=self.ctype,
                    is_const=self.is_const)

    def __str__(self):
        if self.target_fundamental:
            return self.target_fundamental
        elif self.target_giname:
            return self.target_giname
        elif self.target_foreign:
            return self.target_foreign

    def __repr__(self):
        if self.target_fundamental:
            data = 'target_fundamental=%s, ' % (self.target_fundamental, )
        elif self.target_giname:
            data = 'target_giname=%s, ' % (self.target_giname, )
        elif self.target_foreign:
            data = 'target_foreign=%s, ' % (self.target_foreign, )
        else:
            data = ''
        return '%s(%sctype=%s)' % (self.__class__.__name__, data, self.ctype)


class TypeUnknown(Type):
    def __init__(self):
        Type.__init__(self, _target_unknown=True)


# Fundamental types, two special ones
TYPE_NONE = Type(target_fundamental='none', ctype='void')
TYPE_ANY = Type(target_fundamental='gpointer', ctype='gpointer')
# Fundamental types, "Basic" types
TYPE_BOOLEAN = Type(target_fundamental='gboolean', ctype='gboolean')
TYPE_INT8 = Type(target_fundamental='gint8', ctype='gint8')
TYPE_UINT8 = Type(target_fundamental='guint8', ctype='guint8')
TYPE_INT16 = Type(target_fundamental='gint16', ctype='gint16')
TYPE_UINT16 = Type(target_fundamental='guint16', ctype='guint16')
TYPE_INT32 = Type(target_fundamental='gint32', ctype='gint32')
TYPE_UINT32 = Type(target_fundamental='guint32', ctype='guint32')
TYPE_INT64 = Type(target_fundamental='gint64', ctype='gint64')
TYPE_UINT64 = Type(target_fundamental='guint64', ctype='guint64')
TYPE_CHAR = Type(target_fundamental='gchar', ctype='gchar')
TYPE_SHORT = Type(target_fundamental='gshort', ctype='gshort')
TYPE_USHORT = Type(target_fundamental='gushort', ctype='gushort')
TYPE_INT = Type(target_fundamental='gint', ctype='gint')
TYPE_UINT = Type(target_fundamental='guint', ctype='guint')
TYPE_LONG = Type(target_fundamental='glong', ctype='glong')
TYPE_ULONG = Type(target_fundamental='gulong', ctype='gulong')
TYPE_SIZE = Type(target_fundamental='gsize', ctype='gsize')
TYPE_SSIZE = Type(target_fundamental='gssize', ctype='gssize')
TYPE_INTPTR = Type(target_fundamental='gintptr', ctype='gintptr')
TYPE_UINTPTR = Type(target_fundamental='guintptr', ctype='guintptr')
# C99 types
TYPE_LONG_LONG = Type(target_fundamental='long long', ctype='long long')
TYPE_LONG_ULONG = Type(target_fundamental='unsigned long long',
                       ctype='unsigned long long')
TYPE_FLOAT = Type(target_fundamental='gfloat', ctype='gfloat')
TYPE_DOUBLE = Type(target_fundamental='gdouble', ctype='gdouble')
# ?
TYPE_LONG_DOUBLE = Type(target_fundamental='long double',
                        ctype='long double')
TYPE_UNICHAR = Type(target_fundamental='gunichar', ctype='gunichar')
# Platform-specific types
TYPE_TIME_T = Type(target_fundamental='time_t', ctype='time_t')
TYPE_OFF_T = Type(target_fundamental='off_t', ctype='off_t')

# C types with semantics overlaid
TYPE_GTYPE = Type(target_fundamental='GType', ctype='GType')
TYPE_STRING = Type(target_fundamental='utf8', ctype='gchar*')
TYPE_FILENAME = Type(target_fundamental='filename', ctype='gchar*')

TYPE_VALIST = Type(target_fundamental='va_list', ctype='va_list')

BASIC_TYPES = [TYPE_BOOLEAN, TYPE_INT8, TYPE_UINT8, TYPE_INT16,
               TYPE_UINT16, TYPE_INT32, TYPE_UINT32, TYPE_INT64,
               TYPE_UINT64, TYPE_CHAR, TYPE_SHORT, TYPE_USHORT, TYPE_INT,
               TYPE_UINT, TYPE_LONG, TYPE_ULONG, TYPE_SIZE, TYPE_SSIZE,
               TYPE_LONG_LONG, TYPE_LONG_ULONG, TYPE_TIME_T, TYPE_OFF_T,
               TYPE_FLOAT, TYPE_DOUBLE,
               TYPE_LONG_DOUBLE, TYPE_UNICHAR, TYPE_GTYPE]

BASIC_GIR_TYPES = [TYPE_INTPTR, TYPE_UINTPTR]
BASIC_GIR_TYPES.extend(BASIC_TYPES)

GIR_TYPES = [TYPE_NONE, TYPE_ANY]
GIR_TYPES.extend(BASIC_GIR_TYPES)
GIR_TYPES.extend([TYPE_STRING, TYPE_FILENAME, TYPE_VALIST])

# These are the only basic types that are guaranteed to
# be as big as a pointer (and thus are allowed in GPtrArray)
POINTER_TYPES = [TYPE_ANY, TYPE_INTPTR, TYPE_UINTPTR]

INTROSPECTABLE_BASIC = list(GIR_TYPES)
for v in [TYPE_NONE, TYPE_ANY,
          TYPE_LONG_LONG, TYPE_LONG_ULONG,
          TYPE_LONG_DOUBLE, TYPE_VALIST]:
    INTROSPECTABLE_BASIC.remove(v)

type_names = {}
for typeval in GIR_TYPES:
    type_names[typeval.target_fundamental] = typeval
basic_type_names = {}
for typeval in BASIC_GIR_TYPES:
    basic_type_names[typeval.target_fundamental] = typeval

# C builtin
type_names['char'] = TYPE_CHAR
type_names['signed char'] = TYPE_INT8
type_names['unsigned char'] = TYPE_UINT8
type_names['short'] = TYPE_SHORT
type_names['signed short'] = TYPE_SHORT
type_names['unsigned short'] = TYPE_USHORT
type_names['int'] = TYPE_INT
type_names['signed int'] = TYPE_INT
type_names['unsigned short int'] = TYPE_USHORT
type_names['signed'] = TYPE_INT
type_names['unsigned int'] = TYPE_UINT
type_names['unsigned'] = TYPE_UINT
type_names['long'] = TYPE_LONG
type_names['signed long'] = TYPE_LONG
type_names['unsigned long'] = TYPE_ULONG
type_names['unsigned long int'] = TYPE_ULONG
type_names['float'] = TYPE_FLOAT
type_names['double'] = TYPE_DOUBLE
type_names['char*'] = TYPE_STRING
type_names['void*'] = TYPE_ANY
type_names['void'] = TYPE_NONE
# Also alias the signed one here
type_names['signed long long'] = TYPE_LONG_LONG
# C99 stdint exact width types
type_names['int8_t'] = TYPE_INT8
type_names['uint8_t'] = TYPE_UINT8
type_names['int16_t'] = TYPE_INT16
type_names['uint16_t'] = TYPE_UINT16
type_names['int32_t'] = TYPE_INT32
type_names['uint32_t'] = TYPE_UINT32
type_names['int64_t'] = TYPE_INT64
type_names['uint64_t'] = TYPE_UINT64

# A few additional GLib type aliases
type_names['guchar'] = TYPE_UINT8
type_names['gchararray'] = TYPE_STRING
type_names['gchar*'] = TYPE_STRING
type_names['goffset'] = TYPE_INT64
type_names['gunichar2'] = TYPE_UINT16
type_names['gsize'] = TYPE_SIZE
type_names['gssize'] = TYPE_SSIZE
type_names['gintptr'] = TYPE_INTPTR
type_names['guintptr'] = TYPE_UINTPTR
type_names['gconstpointer'] = TYPE_ANY
type_names['grefcount'] = TYPE_INT
type_names['gatomicrefcount'] = TYPE_INT

# We used to support these; continue to do so
type_names['any'] = TYPE_ANY
type_names['boolean'] = TYPE_BOOLEAN
type_names['uint'] = TYPE_UINT
type_names['ulong'] = TYPE_ULONG

# C stdio, used in GLib public headers; squash this for now here
# until we move scanning into GLib and can (skip)
type_names['FILE*'] = TYPE_ANY

# One off C unix type definitions; note some of these may be GNU Libc
# specific.  If someone is actually bitten by this, feel free to do
# the required configure goop to determine their size and replace
# here.
#
# We don't want to encourage people to use these in their APIs because
# they compromise the platform-independence that GLib gives you.
# These are here mostly to avoid blowing when random platform-specific
# methods are added under #ifdefs inside GLib itself.  We could just (skip)
# the relevant methods, but on the other hand, since these types are just
# integers it's easy enough to expand them.
type_names['size_t'] = type_names['gsize']
type_names['ssize_t'] = type_names['gssize']
type_names['uintptr_t'] = type_names['guintptr']
type_names['intptr_t'] = type_names['gintptr']
type_names['time_t'] = TYPE_TIME_T
type_names['off_t'] = TYPE_OFF_T
type_names['pid_t'] = TYPE_INT
type_names['uid_t'] = TYPE_UINT
type_names['gid_t'] = TYPE_UINT
type_names['dev_t'] = TYPE_INT
type_names['socklen_t'] = TYPE_INT32

# Obj-C
type_names['id'] = TYPE_ANY

# Parameters
PARAM_DIRECTION_IN = 'in'
PARAM_DIRECTION_OUT = 'out'
PARAM_DIRECTION_INOUT = 'inout'

PARAM_SCOPE_CALL = 'call'
PARAM_SCOPE_ASYNC = 'async'
PARAM_SCOPE_NOTIFIED = 'notified'
PARAM_SCOPE_FOREVER = 'forever'

PARAM_TRANSFER_NONE = 'none'
PARAM_TRANSFER_CONTAINER = 'container'
PARAM_TRANSFER_FULL = 'full'

SIGNAL_FIRST = 'first'
SIGNAL_LAST = 'last'
SIGNAL_CLEANUP = 'cleanup'
SIGNAL_MUST_COLLECT = 'must-collect'


class Namespace(object):
    def __init__(self, name, version, identifier_prefixes=None, symbol_prefixes=None):
        self.name = name
        self.version = version
        if identifier_prefixes is not None:
            self.identifier_prefixes = identifier_prefixes
        else:
            self.identifier_prefixes = [name]
        if symbol_prefixes is not None:
            self.symbol_prefixes = symbol_prefixes
        else:
            ps = self.identifier_prefixes
            self.symbol_prefixes = [to_underscores(p).lower() for p in ps]
        # cache upper-cased versions
        self._ucase_symbol_prefixes = [p.upper() for p in self.symbol_prefixes]
        self.names = OrderedDict()   # Maps from GIName -> node
        self.aliases = {}            # Maps from GIName -> GIName
        self.type_names = {}         # Maps from GTName -> node
        self.ctypes = {}             # Maps from CType -> node
        self.symbols = {}            # Maps from function symbols -> Function
        # Immediate includes only, not their transitive closure:
        self.includes = set()        # Include
        self.shared_libraries = []   # str
        self.c_includes = []         # str
        self.exported_packages = []  # str

    def type_from_name(self, name, ctype=None):
        """Backwards compatibility method for older .gir files, which
only use the 'name' attribute.  If name refers to a fundamental type,
create a Type object referncing it.  If name is already a
fully-qualified GIName like 'Foo.Bar', returns a Type targeting it .
Otherwise a Type targeting name qualififed with the namespace name is
returned."""
        if name in type_names:
            return Type(target_fundamental=name, ctype=ctype)
        if '.' in name:
            target = name
        else:
            target = '%s.%s' % (self.name, name)
        return Type(target_giname=target, ctype=ctype)

    def track(self, node):
        """Doesn't directly append the function to our own namespace,
but adds it to things like ctypes, symbols, and type_names.
"""
        assert isinstance(node, Node)
        if node.namespace is self:
            return
        assert node.namespace is None
        node.namespace = self
        if isinstance(node, Alias):
            self.aliases[node.name] = node
        elif isinstance(node, Registered) and node.gtype_name is not None:
            self.type_names[node.gtype_name] = node
        elif isinstance(node, Function):
            self.symbols[node.symbol] = node
        if isinstance(node, (Compound, Class, Interface, Boxed)):
            for fn in chain(node.methods, node.static_methods, node.constructors):
                if not isinstance(fn, Function):
                    continue
                fn.namespace = self
                self.symbols[fn.symbol] = fn
        if isinstance(node, (Compound, Class, Interface)):
            for f in node.fields:
                f.namespace = self
        if isinstance(node, (Class, Interface)):
            for m in chain(node.signals, node.properties):
                m.namespace = self
        if isinstance(node, (Enum, Bitfield)):
            for fn in node.static_methods:
                if not isinstance(fn, Function):
                    continue
                fn.namespace = self
                self.symbols[fn.symbol] = fn
            for member in node.members:
                member.namespace = self
                self.symbols[member.symbol] = member
        if hasattr(node, 'ctype'):
            self.ctypes[node.ctype] = node

    def append(self, node, replace=False):
        previous = self.names.get(node.name)
        if previous is not None:
            if not replace:
                raise ValueError("Namespace conflict: %r" % (node, ))
            self.remove(previous)

        self.track(node)
        self.names[node.name] = node

    def remove(self, node):
        if isinstance(node, Alias):
            del self.aliases[node.name]
        elif isinstance(node, Registered) and node.gtype_name is not None:
            del self.type_names[node.gtype_name]
        if hasattr(node, 'ctype'):
            del self.ctypes[node.ctype]
        if isinstance(node, Function):
            del self.symbols[node.symbol]
        node.namespace = None
        self.names.pop(node.name, None)

    def float(self, node):
        """Like remove(), but doesn't unset the node's namespace
back-reference, and it's still possible to look up
functions via get_by_symbol()."""
        if isinstance(node, Function):
            symbol = node.symbol
        self.remove(node)
        self.symbols[symbol] = node
        node.namespace = self

    def __iter__(self):
        return iter(self.names)

    def items(self):
        return self.names.items()

    def values(self):
        return self.names.values()

    def get(self, name):
        return self.names.get(name)

    def get_by_ctype(self, ctype):
        return self.ctypes.get(ctype)

    def get_by_symbol(self, symbol):
        return self.symbols.get(symbol)

    def walk(self, callback):
        for node in self.values():
            node.walk(callback, [])


class Include(object):

    def __init__(self, name, version):
        self.name = name
        self.version = version

    @classmethod
    def from_string(cls, string):
        return cls(*string.split('-', 1))

    def _compare(self, other, op):
        return op((self.name, self.version), (other.name, other.version))

    def __lt__(self, other):
        return self._compare(other, operator.lt)

    def __gt__(self, other):
        return self._compare(other, operator.gt)

    def __ge__(self, other):
        return self._compare(other, operator.ge)

    def __le__(self, other):
        return self._compare(other, operator.le)

    def __eq__(self, other):
        return self._compare(other, operator.eq)

    def __ne__(self, other):
        return self._compare(other, operator.ne)

    def __hash__(self):
        return hash(str(self))

    def __str__(self):
        return '%s-%s' % (self.name, self.version)


class Annotated(object):
    """An object which has a few generic metadata
properties."""
    def __init__(self):
        self.version = None
        self.version_doc = None
        self.skip = False
        self.introspectable = True
        self.attributes = OrderedDict()
        self.stability = None
        self.stability_doc = None
        self.deprecated = None
        self.deprecated_doc = None
        self.doc = None
        self.doc_position = None


class Node(Annotated):
    """A node is a type of object which is uniquely identified by its
(namespace, name) pair.  When combined with a ., this is called a
GIName.  It's possible for nodes to contain or point to other nodes."""

    c_name = property(lambda self: self.namespace.name + self.name if self.namespace else
                      self.name)
    gi_name = property(lambda self: '%s.%s' % (self.namespace.name, self.name))

    def __init__(self, name=None):
        Annotated.__init__(self)
        self.namespace = None   # Should be set later by Namespace.append()
        self.name = name
        self.foreign = False
        self.file_positions = set()
        self._parent = None

    def _get_parent(self):
        if self._parent is not None:
            return self._parent
        else:
            return self.namespace

    def _set_parent(self, value):
        self._parent = value
    parent = property(_get_parent, _set_parent)

    def create_type(self):
        """Create a Type object referencing this node."""
        assert self.namespace is not None
        return Type(target_giname=('%s.%s' % (self.namespace.name, self.name)))

    def _compare(self, other, op):
        return op((self.namespace, self.name), (other.namespace, other.name))

    def __lt__(self, other):
        return self._compare(other, operator.lt)

    def __gt__(self, other):
        return self._compare(other, operator.gt)

    def __ge__(self, other):
        return self._compare(other, operator.ge)

    def __le__(self, other):
        return self._compare(other, operator.le)

    def __eq__(self, other):
        return self._compare(other, operator.eq)

    def __ne__(self, other):
        return self._compare(other, operator.ne)

    def __hash__(self):
        return hash((self.namespace, self.name))

    def __repr__(self):
        return "%s('%s')" % (self.__class__.__name__, self.name)

    def inherit_file_positions(self, node):
        self.file_positions.update(node.file_positions)

    def add_file_position(self, position):
        self.file_positions.add(position)

    def get_main_position(self):
        if not self.file_positions:
            return None

        res = None
        for position in self.file_positions:
            if position.is_typedef:
                res = position
            else:
                return position

        return res

    def add_symbol_reference(self, symbol):
        if symbol.source_filename:
            self.add_file_position(Position(symbol.source_filename, symbol.line,
                is_typedef=symbol.type in (CTYPE_TYPEDEF, CSYMBOL_TYPE_TYPEDEF)))

    def walk(self, callback, chain):
        res = callback(self, chain)
        assert res in (True, False), "Walk function must return boolean, not %r" % (res, )
        if not res:
            return False
        chain.append(self)
        self._walk(callback, chain)
        chain.pop()

    def _walk(self, callback, chain):
        pass


class DocSection(Node):
    def __init__(self, name=None):
        Node.__init__(self, name)


class Registered:
    """A node that (possibly) has gtype_name and get_type."""
    def __init__(self, gtype_name, get_type):
        assert (gtype_name is None and get_type is None) or \
               (gtype_name is not None and get_type is not None)
        self.gtype_name = gtype_name
        self.get_type = get_type


class Callable(Node):

    def __init__(self, name, retval, parameters, throws):
        Node.__init__(self, name)
        self.retval = retval
        self.parameters = parameters
        self.throws = not not throws
        self.instance_parameter = None  # Parameter
        self.parent = None  # A Class or Interface
        self.finish_func = None
        self.sync_func = None
        self.async_func = None

    def _get_retval(self):
        return self._retval

    def _set_retval(self, value):
        self._retval = value
        if self._retval is not None:
            self._retval.parent = self
    retval = property(_get_retval, _set_retval)

    def _get_instance_parameter(self):
        return self._instance_parameter

    def _set_instance_parameter(self, value):
        self._instance_parameter = value
        if value is not None:
            value.parent = self
    instance_parameter = property(_get_instance_parameter,
                                  _set_instance_parameter)

    def _get_parameters(self):
        return self._parameters

    def _set_parameters(self, value):
        self._parameters = value
        for param in self._parameters:
            param.parent = self
    parameters = property(_get_parameters, _set_parameters)

    # Returns all parameters, including the instance parameter
    @property
    def all_parameters(self):
        if self.instance_parameter is not None:
            return [self.instance_parameter] + self.parameters
        else:
            return self.parameters

    def get_parameter_index(self, name):
        for i, parameter in enumerate(self.parameters):
            if parameter.argname == name:
                return i
        raise ValueError("Unknown argument %s" % (name, ))

    def get_parameter(self, name):
        for parameter in self.all_parameters:
            if parameter.argname == name:
                return parameter
        raise ValueError("Unknown argument %s" % (name, ))


class FunctionMacro(Node):
    def __init__(self, name, parameters, symbol):
        Node.__init__(self, name)
        self.symbol = symbol
        self.parameters = parameters
        self.introspectable = False


class Function(Callable):

    def __init__(self, name, retval, parameters, throws, symbol):
        Callable.__init__(self, name, retval, parameters, throws)
        self.symbol = symbol
        self.is_method = False
        self.is_constructor = False
        self.shadowed_by = None         # C symbol string
        self.shadows = None             # C symbol string
        self.moved_to = None            # namespaced function name string
        self.internal_skipped = False   # if True, this func will not be written to GIR
        self.set_property = None        # Property name
        self.get_property = None        # Property name
        self.is_inline = False

    def clone(self):
        clone = copy.copy(self)
        # copy the parameters array so a change to self.parameters does not
        # influence clone.parameters.
        clone.parameters = self.parameters[:]
        for param in clone.parameters:
            param.parent = clone
        return clone

    def is_type_meta_function(self):
        # Named correctly
        if not (self.name.endswith('_get_type') or self.name.endswith('_get_gtype')):
            return False

        # Doesn't have any parameters
        if self.parameters:
            return False

        # Returns GType
        rettype = self.retval.type
        if (not rettype.is_equiv(TYPE_GTYPE) and rettype.target_giname != 'Gtk.Type'):
            warn("function '%s' returns '%r', not a GType" % (self.name, rettype))
            return False

        return True


class ErrorQuarkFunction(Function):

    def __init__(self, name, retval, parameters, throws, symbol, error_domain):
        Function.__init__(self, name, retval, parameters, throws, symbol)
        self.error_domain = error_domain


class VFunction(Callable):

    def __init__(self, name, retval, parameters, throws):
        Callable.__init__(self, name, retval, parameters, throws)
        self.invoker = None

    @classmethod
    def from_callback(cls, name, cb):
        obj = cls(name, cb.retval, cb.parameters[1:],
                  cb.throws)
        return obj


class Varargs(Type):

    def __init__(self):
        Type.__init__(self, '<varargs>', target_fundamental='<varargs>')


class Array(Type):
    C = '<c>'
    GLIB_ARRAY = 'GLib.Array'
    GLIB_BYTEARRAY = 'GLib.ByteArray'
    GLIB_PTRARRAY = 'GLib.PtrArray'

    def __init__(self, array_type, element_type, **kwargs):
        Type.__init__(self, target_fundamental='<array>',
                      **kwargs)
        if (array_type is None or array_type == self.C):
            self.array_type = self.C
        else:
            assert array_type in (self.GLIB_ARRAY,
                                  self.GLIB_BYTEARRAY,
                                  self.GLIB_PTRARRAY), array_type
            self.array_type = array_type
        assert isinstance(element_type, Type)
        self.element_type = element_type
        self.zeroterminated = True
        self.length_param_name = None
        self.size = None

    def clone(self):
        arr = Array(self.array_type, self.element_type)
        arr.zeroterminated = self.zeroterminated
        arr.length_param_name = self.length_param_name
        arr.size = self.size
        return arr


class List(Type):

    def __init__(self, name, element_type, **kwargs):
        Type.__init__(self, target_fundamental='<list>',
                      **kwargs)
        self.name = name
        assert isinstance(element_type, Type)
        self.element_type = element_type

    def clone(self):
        return List(self.name, self.element_type)


class Map(Type):

    def __init__(self, key_type, value_type, **kwargs):
        Type.__init__(self, target_fundamental='<map>', **kwargs)
        assert isinstance(key_type, Type)
        self.key_type = key_type
        assert isinstance(value_type, Type)
        self.value_type = value_type

    def clone(self):
        return Map(self.key_type, self.value_type)


class Alias(Node):

    def __init__(self, name, target, ctype=None):
        Node.__init__(self, name)
        self.target = target
        self.ctype = ctype


class TypeContainer(Annotated):
    """A fundamental base class for Return and Parameter."""

    def __init__(self, typenode, nullable, not_nullable, transfer, direction):
        Annotated.__init__(self)
        self.type = typenode
        self.nullable = nullable
        self.not_nullable = not_nullable
        self.direction = direction
        if transfer is not None:
            self.transfer = transfer
        elif typenode and typenode.is_const:
            self.transfer = PARAM_TRANSFER_NONE
        else:
            self.transfer = None


class Parameter(TypeContainer):
    """An argument to a function."""

    def __init__(self, argname, typenode, direction=None,
                 transfer=None, nullable=False, optional=False,
                 allow_none=False, scope=None,
                 caller_allocates=False, not_nullable=False):
        TypeContainer.__init__(self, typenode, nullable, not_nullable,
                               transfer, direction)
        self.argname = argname
        self.optional = optional
        self.parent = None  # A Callable

        if allow_none:
            if self.direction == PARAM_DIRECTION_OUT:
                self.optional = True
            else:
                self.nullable = True

        self.scope = scope
        self.caller_allocates = caller_allocates
        self.closure_name = None
        self.destroy_name = None

    @property
    def name(self):
        return self.argname


class Return(TypeContainer):
    """A return value from a function."""

    def __init__(self, rtype, nullable=False, not_nullable=False,
                 transfer=None):
        TypeContainer.__init__(self, rtype, nullable, not_nullable, transfer,
                               direction=PARAM_DIRECTION_OUT)
        self.parent = None  # A Callable


class Enum(Node, Registered):

    def __init__(self, name, ctype,
                 gtype_name=None,
                 get_type=None,
                 c_symbol_prefix=None,
                 members=None):
        Node.__init__(self, name)
        Registered.__init__(self, gtype_name, get_type)
        self.c_symbol_prefix = c_symbol_prefix
        self.ctype = ctype
        self.members = members
        for member in members:
            member.parent = self
        # Associated error domain name
        self.error_domain = None
        self.static_methods = []

    def _walk(self, callback, chain):
        for meth in self.static_methods:
            meth.walk(callback, chain)


class Bitfield(Node, Registered):

    def __init__(self, name, ctype,
                 gtype_name=None,
                 c_symbol_prefix=None,
                 get_type=None,
                 members=None):
        Node.__init__(self, name)
        Registered.__init__(self, gtype_name, get_type)
        self.ctype = ctype
        self.c_symbol_prefix = c_symbol_prefix
        self.members = members
        for member in members:
            member.parent = self
        self.static_methods = []

    def _walk(self, callback, chain):
        for meth in self.static_methods:
            meth.walk(callback, chain)


class Member(Annotated):

    def __init__(self, name, value, symbol, nick=None, dump_name=None):
        Annotated.__init__(self)
        self.name = name
        self.value = value
        self.symbol = symbol
        self.nick = nick
        self.dump_name = dump_name
        self.parent = None

    def _compare(self, other, op):
        return op(self.name, other.name)

    def __lt__(self, other):
        return self._compare(other, operator.lt)

    def __gt__(self, other):
        return self._compare(other, operator.gt)

    def __ge__(self, other):
        return self._compare(other, operator.ge)

    def __le__(self, other):
        return self._compare(other, operator.le)

    def __eq__(self, other):
        return self._compare(other, operator.eq)

    def __ne__(self, other):
        return self._compare(other, operator.ne)

    def __hash__(self):
        return hash(self.name)

    def __repr__(self):
        return "%s('%s')" % (self.__class__.__name__, self.name)


class Compound(Node, Registered):
    def __init__(self, name,
                 ctype=None,
                 gtype_name=None,
                 get_type=None,
                 c_symbol_prefix=None,
                 disguised=False,
                 opaque=False,
                 pointer=False,
                 tag_name=None):
        Node.__init__(self, name)
        Registered.__init__(self, gtype_name, get_type)
        self.ctype = ctype
        self.methods = []
        self.static_methods = []
        self.fields = []
        self.constructors = []
        self.disguised = disguised
        self.opaque = opaque
        self.pointer = pointer
        self.gtype_name = gtype_name
        self.get_type = get_type
        self.c_symbol_prefix = c_symbol_prefix
        self.tag_name = tag_name

    def add_gtype(self, gtype_name, get_type):
        self.gtype_name = gtype_name
        self.get_type = get_type
        self.namespace.type_names[gtype_name] = self

    def _walk(self, callback, chain):
        for ctor in self.constructors:
            ctor.walk(callback, chain)
        for func in self.methods:
            func.walk(callback, chain)
        for func in self.static_methods:
            func.walk(callback, chain)
        for field in self.fields:
            if field.anonymous_node is not None:
                field.anonymous_node.walk(callback, chain)

    def get_field(self, name):
        for field in self.fields:
            if field.name == name:
                return field
        raise ValueError("Unknown field %s" % (name, ))

    def get_field_index(self, name):
        for i, field in enumerate(self.fields):
            if field.name == name:
                return i
        raise ValueError("Unknown field %s" % (name, ))


class Field(Annotated):

    def __init__(self, name, typenode, readable, writable, bits=None,
                 anonymous_node=None):
        Annotated.__init__(self)
        assert (typenode or anonymous_node)
        self.name = name
        self.type = typenode
        self.readable = readable
        self.writable = writable
        self.bits = bits
        self.anonymous_node = anonymous_node
        self.private = False
        self.namespace = None
        self.parent = None  # a compound

    def _compare(self, other, op):
        return op(self.name, other.name)

    def __lt__(self, other):
        return self._compare(other, operator.lt)

    def __gt__(self, other):
        return self._compare(other, operator.gt)

    def __ge__(self, other):
        return self._compare(other, operator.ge)

    def __le__(self, other):
        return self._compare(other, operator.le)

    def __eq__(self, other):
        return self._compare(other, operator.eq)

    def __ne__(self, other):
        return self._compare(other, operator.ne)

    def __hash__(self):
        return hash(self.name)

    def __repr__(self):
        return "%s('%s')" % (self.__class__.__name__, self.name)


class Record(Compound):

    def __init__(self, name,
                 ctype=None,
                 gtype_name=None,
                 get_type=None,
                 c_symbol_prefix=None,
                 disguised=False,
                 opaque=False,
                 pointer=False,
                 tag_name=None):
        Compound.__init__(self, name,
                          ctype=ctype,
                          gtype_name=gtype_name,
                          get_type=get_type,
                          c_symbol_prefix=c_symbol_prefix,
                          disguised=disguised,
                          opaque=opaque,
                          pointer=pointer,
                          tag_name=tag_name)
        # If non-None, this record defines the FooClass C structure
        # for some Foo GObject (or similar for GInterface)
        self.is_gtype_struct_for = None
        # If non-None, this record has a copy function for heap
        # allocated instances
        self.copy_func = None
        # If non-None, this record has a free function for heap
        # allocated instances
        self.free_func = None


class Union(Compound):

    def __init__(self, name,
                 ctype=None,
                 gtype_name=None,
                 get_type=None,
                 c_symbol_prefix=None,
                 disguised=False,
                 opaque=False,
                 pointer=False,
                 tag_name=None):
        Compound.__init__(self, name,
                          ctype=ctype,
                          gtype_name=gtype_name,
                          get_type=get_type,
                          c_symbol_prefix=c_symbol_prefix,
                          disguised=disguised,
                          opaque=opaque,
                          pointer=pointer,
                          tag_name=tag_name)
        # If non-None, this union has a copy function for heap
        # allocated instances
        self.copy_func = None
        # If non-None, this union has a free function for heap
        # allocated instances
        self.free_func = None


class Boxed(Node, Registered):
    """A boxed type with no known associated structure/union."""
    def __init__(self, name,
                 gtype_name=None,
                 get_type=None,
                 c_symbol_prefix=None):
        assert gtype_name is not None
        assert get_type is not None
        Node.__init__(self, name)
        Registered.__init__(self, gtype_name, get_type)
        if get_type is not None:
            assert c_symbol_prefix is not None
        self.c_symbol_prefix = c_symbol_prefix
        self.constructors = []
        self.methods = []
        self.static_methods = []

    def _walk(self, callback, chain):
        for ctor in self.constructors:
            ctor.walk(callback, chain)
        for meth in self.methods:
            meth.walk(callback, chain)
        for meth in self.static_methods:
            meth.walk(callback, chain)


class Signal(Callable):

    def __init__(self, name, retval, parameters, when=None,
                 no_recurse=False, detailed=False, action=False,
                 no_hooks=False):
        Callable.__init__(self, name, retval, parameters, False)
        self.when = when
        self.no_recurse = no_recurse
        self.detailed = detailed
        self.action = action
        self.no_hooks = no_hooks
        self.emitter = None


class Class(Node, Registered):

    def __init__(self, name, parent_type,
                 ctype=None,
                 gtype_name=None,
                 get_type=None,
                 c_symbol_prefix=None,
                 is_abstract=False,
                 is_final=False):
        Node.__init__(self, name)
        Registered.__init__(self, gtype_name, get_type)
        self.ctype = ctype
        self.c_symbol_prefix = c_symbol_prefix
        self.parent_type = parent_type
        self.fundamental = False
        self.unref_func = None
        self.ref_func = None
        self.set_value_func = None
        self.get_value_func = None
        # When we're in the scanner, we keep around a list
        # of parents so that we can transparently fall back
        # if there are 'hidden' parents
        self.parent_chain = []
        self.glib_type_struct = None
        self.is_abstract = is_abstract
        self.is_final = is_final
        self.methods = []
        self.virtual_methods = []
        self.static_methods = []
        self.interfaces = []
        self.constructors = []
        self.properties = []
        self.fields = []
        self.signals = []

    def _walk(self, callback, chain):
        for meth in self.methods:
            meth.walk(callback, chain)
        for meth in self.virtual_methods:
            meth.walk(callback, chain)
        for meth in self.static_methods:
            meth.walk(callback, chain)
        for ctor in self.constructors:
            ctor.walk(callback, chain)
        for field in self.fields:
            if field.anonymous_node:
                field.anonymous_node.walk(callback, chain)
        for sig in self.signals:
            sig.walk(callback, chain)
        for prop in self.properties:
            prop.walk(callback, chain)


class Interface(Node, Registered):

    def __init__(self, name, parent_type,
                 ctype=None,
                 gtype_name=None,
                 get_type=None,
                 c_symbol_prefix=None):
        Node.__init__(self, name)
        Registered.__init__(self, gtype_name, get_type)
        self.ctype = ctype
        self.c_symbol_prefix = c_symbol_prefix
        self.parent_type = parent_type
        self.parent_chain = []
        self.methods = []
        self.signals = []
        self.static_methods = []
        self.virtual_methods = []
        self.glib_type_struct = None
        self.properties = []
        self.fields = []
        self.prerequisites = []
        # Not used yet, exists just to avoid an exception in
        # Namespace.append()
        self.constructors = []

    def _walk(self, callback, chain):
        for meth in self.methods:
            meth.walk(callback, chain)
        for meth in self.static_methods:
            meth.walk(callback, chain)
        for meth in self.virtual_methods:
            meth.walk(callback, chain)
        for field in self.fields:
            if field.anonymous_node:
                field.anonymous_node.walk(callback, chain)
        for sig in self.signals:
            sig.walk(callback, chain)


class Constant(Node):

    def __init__(self, name, value_type, value, ctype):
        Node.__init__(self, name)
        self.value_type = value_type
        self.value = value
        self.ctype = ctype


class Property(Node):

    def __init__(self, name, typeobj, readable, writable,
                 construct, construct_only, transfer=None):
        Node.__init__(self, name)
        self.type = typeobj
        self.readable = readable
        self.writable = writable
        self.construct = construct
        self.construct_only = construct_only
        if transfer is None:
            self.transfer = PARAM_TRANSFER_NONE
        else:
            self.transfer = transfer
        self.setter = None
        self.getter = None
        self.default_value = None
        self.parent = None  # A Class or Interface


class Callback(Callable):

    def __init__(self, name, retval, parameters, throws, ctype=None):
        Callable.__init__(self, name, retval, parameters, throws)
        self.ctype = ctype
