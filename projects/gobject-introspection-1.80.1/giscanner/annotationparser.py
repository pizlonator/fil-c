# -*- coding: utf-8 -*-
# -*- Mode: Python -*-

# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2008-2010 Johan Dahlin
# Copyright (C) 2012-2013 Dieter Verfaillie <dieterv@optionexplicit.be>
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


'''
GTK-Doc comment block format
----------------------------

A GTK-Doc comment block is built out of multiple parts. Each part can be further
divided into fields which are separated by a colon ("``:``") delimiter.

Known parts and the fields they are constructed from look like the following
(optional fields are enclosed in square brackets)::

    ┌───────────────────────────────────────────────────────────┐
    │ /**                                                       │ ─▷ start token
    ├────────────────────┬──────────────────────────────────────┤
    │  * identifier_name │ [: annotations]                      │ ─▷ identifier part
    ├────────────────────┼─────────────────┬────────────────────┤
    │  * @parameter_name │ [: annotations] │ : description      │ ─▷ parameter part
    ├────────────────────┴─────────────────┴────────────────────┤
    │  *                                                        │ ─▷ comment block description
    │  * comment_block_description                              │
    ├─────────────┬─────────────────┬───────────┬───────────────┤
    │  * tag_name │ [: annotations] │ [: value] │ : description │ ─▷ tag part
    ├─────────────┴─────────────────┴───────────┴───────────────┤
    │  */                                                       │ ─▷ end token
    └───────────────────────────────────────────────────────────┘

There are two conditions that must be met before a comment block is recognized
as a GTK-Doc comment block:

#. The comment block is opened with a GTK-Doc start token ("``/**``")
#. The first line following the start token contains a valid identifier part

Once a GTK-Doc comment block has been identified as such and has been stripped
from its start and end tokens the remaining parts have to be written in a
specific order:

#. There must be exactly 1 `identifier` part on the first line of the
   comment block which consists of:

   * a required `identifier_name` field
   * an optional `annotations` field, optionally spanning multiple lines

#. Zero or more `parameter` parts, each consisting of:

   * a required `parameter_name` field
   * an optional `annotations` field, optionally spanning multiple lines
   * a required `description` field (can be the empty string)

#. One optional `comment block description` part which must begin with at
   least 1 empty line signaling the start of this part.

#. Zero or more `tag` parts, each consisting of:

   * a required `tag_name` field
   * an optional `annotations` field, optionally spanning multiple lines
   * an optional `value` field
   * a required `description` field (can be the empty string)

Additionally, the following restrictions are in effect:

#. Separating parts with an empty line:

   * `identifier` and `parameter` parts cannot be separated from each other by
     an empty line as this would signal the start of the
     `comment block description` part (see above).
   * it is required to separate the `comment block description` part from the
     `identifier` or `parameter` parts with an empty line (see above)
   * `comment block description` and `tag` parts can optionally be separated
     by an empty line

#. Parts and fields cannot span multiple lines, except for:

   * the `comment_block_description` part
   * `parameter description` and `tag description` fields
   * `identifier`, `parameter` and `tag` part `annotations` fields

#. Taking the above restrictions into account, spanning multiple paragraphs is
   limited to the `comment block description` part and `tag description` fields.

Refer to the `GTK-Doc manual`_ for more detailed usage information.

.. _GTK-Doc manual:
        http://developer-old.gnome.org/gtk-doc-manual/1.18/documenting.html.en
'''

import os
import re
import operator

from collections import namedtuple, Counter, OrderedDict
from operator import ne, gt, lt
from typing import Tuple  # noqa

from .message import Position, warn, error


# GTK-Doc comment block parts
PART_IDENTIFIER = 0
PART_PARAMETERS = 1
PART_DESCRIPTION = 2
PART_TAGS = 3

# GTK-Doc comment block tags
#   1) Basic GTK-Doc tags.
#      Note: This list cannot be extended unless the GTK-Doc project defines new tags.
TAG_DEPRECATED = 'deprecated'
TAG_RETURNS = 'returns'
TAG_SINCE = 'since'
TAG_STABILITY = 'stability'

GTKDOC_TAGS = [TAG_DEPRECATED,
               TAG_RETURNS,
               TAG_SINCE,
               TAG_STABILITY]

#   2) Deprecated basic GTK-Doc tags.
#      Note: This list cannot be extended unless the GTK-Doc project defines new deprecated tags.
TAG_DESCRIPTION = 'description'
TAG_RETURN_VALUE = 'return value'

DEPRECATED_GTKDOC_TAGS = [TAG_DESCRIPTION,
                          TAG_RETURN_VALUE]

#   3) Deprecated GObject-Introspection tags.
#      Unfortunately, these where accepted by old versions of this module.
TAG_RETURN = 'return'
TAG_RETURNS_VALUE = 'returns value'

DEPRECATED_GI_TAGS = [TAG_RETURN,
                      TAG_RETURNS_VALUE]

#   4) Deprecated GObject-Introspection annotation tags.
#      Accepted by old versions of this module while they should have been
#      annotations on the identifier part instead.
#      Note: This list can not be extended ever again. The GObject-Introspection project is not
#            allowed to invent GTK-Doc tags. Please create new annotations instead.
TAG_ATTRIBUTES = 'attributes'
TAG_GET_VALUE_FUNC = 'get value func'
TAG_REF_FUNC = 'ref func'
TAG_RENAME_TO = 'rename to'
TAG_SET_VALUE_FUNC = 'set value func'
TAG_TRANSFER = 'transfer'
TAG_TYPE = 'type'
TAG_UNREF_FUNC = 'unref func'
TAG_VALUE = 'value'
TAG_VFUNC = 'virtual'

DEPRECATED_GI_ANN_TAGS = [TAG_ATTRIBUTES,
                          TAG_GET_VALUE_FUNC,
                          TAG_REF_FUNC,
                          TAG_RENAME_TO,
                          TAG_SET_VALUE_FUNC,
                          TAG_TRANSFER,
                          TAG_TYPE,
                          TAG_UNREF_FUNC,
                          TAG_VALUE,
                          TAG_VFUNC]

ALL_TAGS = GTKDOC_TAGS + DEPRECATED_GTKDOC_TAGS + DEPRECATED_GI_TAGS + DEPRECATED_GI_ANN_TAGS

# GObject-Introspection annotation start/end tokens
ANN_LPAR = '('
ANN_RPAR = ')'

# GObject-Introspection annotations
#   1) Supported annotations
#      Note: when adding new annotations, GTK-Doc project's gtkdoc-mkdb needs to be modified too!
ANN_ALLOW_NONE = 'allow-none'
ANN_ARRAY = 'array'
ANN_ASYNC_FUNC = 'async-func'
ANN_ATTRIBUTES = 'attributes'
ANN_CLOSURE = 'closure'
ANN_CONSTRUCTOR = 'constructor'
ANN_COPY_FUNC = 'copy-func'
ANN_DEFAULT_VALUE = 'default-value'
ANN_DESTROY = 'destroy'
ANN_ELEMENT_TYPE = 'element-type'
ANN_EMITTER = 'emitter'
ANN_FINISH_FUNC = 'finish-func'
ANN_FOREIGN = 'foreign'
ANN_FREE_FUNC = 'free-func'
ANN_GET_PROPERTY = 'get-property'
ANN_GET_VALUE_FUNC = 'get-value-func'
ANN_GETTER = 'getter'
ANN_IN = 'in'
ANN_INOUT = 'inout'
ANN_METHOD = 'method'
ANN_NULLABLE = 'nullable'
ANN_OPTIONAL = 'optional'
ANN_NOT = 'not'
ANN_OUT = 'out'
ANN_REF_FUNC = 'ref-func'
ANN_RENAME_TO = 'rename-to'
ANN_SCOPE = 'scope'
ANN_SET_PROPERTY = 'set-property'
ANN_SET_VALUE_FUNC = 'set-value-func'
ANN_SETTER = 'setter'
ANN_SKIP = 'skip'
ANN_SYNC_FUNC = 'sync-func'
ANN_TRANSFER = 'transfer'
ANN_TYPE = 'type'
ANN_UNREF_FUNC = 'unref-func'
ANN_VFUNC = 'virtual'
ANN_VALUE = 'value'

GI_ANNS = [ANN_ALLOW_NONE,
           ANN_NULLABLE,
           ANN_OPTIONAL,
           ANN_NOT,
           ANN_ARRAY,
           ANN_ASYNC_FUNC,
           ANN_ATTRIBUTES,
           ANN_CLOSURE,
           ANN_CONSTRUCTOR,
           ANN_DEFAULT_VALUE,
           ANN_DESTROY,
           ANN_ELEMENT_TYPE,
           ANN_EMITTER,
           ANN_FINISH_FUNC,
           ANN_FOREIGN,
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
           ANN_SYNC_FUNC,
           ANN_TRANSFER,
           ANN_TYPE,
           ANN_UNREF_FUNC,
           ANN_VFUNC,
           ANN_VALUE]

#   2) Deprecated GObject-Introspection annotations
ANN_ATTRIBUTE = 'attribute'
ANN_INOUT_ALT = 'in-out'

DEPRECATED_GI_ANNS = [ANN_ATTRIBUTE,
                      ANN_INOUT_ALT]

ALL_ANNOTATIONS = GI_ANNS + DEPRECATED_GI_ANNS
DICT_ANNOTATIONS = [ANN_ARRAY, ANN_ATTRIBUTES]
LIST_ANNOTATIONS = [ann for ann in ALL_ANNOTATIONS if ann not in DICT_ANNOTATIONS]

# (array) annotation options
OPT_ARRAY_FIXED_SIZE = 'fixed-size'
OPT_ARRAY_LENGTH = 'length'
OPT_ARRAY_ZERO_TERMINATED = 'zero-terminated'

ARRAY_OPTIONS = [OPT_ARRAY_FIXED_SIZE,
                 OPT_ARRAY_LENGTH,
                 OPT_ARRAY_ZERO_TERMINATED]

# (out) annotation options
OPT_OUT_CALLEE_ALLOCATES = 'callee-allocates'
OPT_OUT_CALLER_ALLOCATES = 'caller-allocates'

OUT_OPTIONS = [OPT_OUT_CALLEE_ALLOCATES,
               OPT_OUT_CALLER_ALLOCATES]

# (not) annotation options
OPT_NOT_NULLABLE = 'nullable'
OPT_NOT_OPTIONAL = 'optional'

NOT_OPTIONS = [OPT_NOT_NULLABLE, OPT_NOT_OPTIONAL]

# (scope) annotation options
OPT_SCOPE_ASYNC = 'async'
OPT_SCOPE_CALL = 'call'
OPT_SCOPE_NOTIFIED = 'notified'
OPT_SCOPE_FOREVER = 'forever'

SCOPE_OPTIONS = [OPT_SCOPE_ASYNC,
                 OPT_SCOPE_CALL,
                 OPT_SCOPE_NOTIFIED,
                 OPT_SCOPE_FOREVER]

# (transfer) annotation options
OPT_TRANSFER_CONTAINER = 'container'
OPT_TRANSFER_FLOATING = 'floating'
OPT_TRANSFER_FULL = 'full'
OPT_TRANSFER_NONE = 'none'

TRANSFER_OPTIONS = [OPT_TRANSFER_CONTAINER,
                    OPT_TRANSFER_FLOATING,
                    OPT_TRANSFER_FULL,
                    OPT_TRANSFER_NONE]


# Pattern used to normalize different types of line endings
LINE_BREAK_RE = re.compile(r'\r\n|\r|\n', re.UNICODE)

# Pattern matching the start token of a comment block.
COMMENT_BLOCK_START_RE = re.compile(
    r'''
    ^                                                    # start
    (?P<code>.*?)                                        # whitespace, code, ...
    \s*                                                  # 0 or more whitespace characters
    (?P<token>/\*{2}(?![\*/]))                           # 1 forward slash character followed
                                                         #   by exactly 2 asterisk characters
                                                         #   and not followed by a slash character
    \s*                                                  # 0 or more whitespace characters
    (?P<comment>.*?)                                     # GTK-Doc comment text
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching the end token of a comment block.
COMMENT_BLOCK_END_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<comment>.*?)                                     # GTK-Doc comment text
    \s*                                                  # 0 or more whitespace characters
    (?P<token>\*+/)                                      # 1 or more asterisk characters followed
                                                         #   by exactly 1 forward slash character
    (?P<code>.*?)                                        # whitespace, code, ...
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching the ' * ' at the beginning of every
# line inside a comment block.
COMMENT_ASTERISK_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<comment>.*?)                                     # invalid comment text
    \s*                                                  # 0 or more whitespace characters
    \*                                                   # 1 asterisk character
    \s?                                                  # 0 or 1 whitespace characters
                                                         #   WARNING: removing more than 1
                                                         #   whitespace character breaks
                                                         #   embedded example program indentation
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching the indentation level of a line (used
# to get the indentation before and after the ' * ').
INDENTATION_RE = re.compile(
    r'''
    ^
    (?P<indentation>\s*)                                 # 0 or more whitespace characters
    .*
    $
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching an empty line.
EMPTY_LINE_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching SECTION identifiers.
SECTION_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    SECTION                                              # SECTION
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<section_name>\w\S+?)                             # section name
    \s*                                                  # 0 or more whitespace characters
    :?                                                   # invalid delimiter
    \s*                                                  # 0 or more whitespace characters
    $
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching symbol (function, constant, struct and enum) identifiers.
SYMBOL_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<symbol_name>[\w-]*\w)                            # symbol name
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<fields>.*?)                                      # annotations + description
    \s*                                                  # 0 or more whitespace characters
    :?                                                   # invalid delimiter
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching property identifiers.
PROPERTY_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<class_name>[\w]+)                                # class name
    \s*                                                  # 0 or more whitespace characters
    :{1}                                                 # 1 required colon
    \s*                                                  # 0 or more whitespace characters
    (?P<property_name>[\w-]*\w)                          # property name
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<fields>.*?)                                      # annotations + description
    \s*                                                  # 0 or more whitespace characters
    :?                                                   # invalid delimiter
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching signal identifiers.
SIGNAL_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<class_name>[\w]+)                                # class name
    \s*                                                  # 0 or more whitespace characters
    :{2}                                                 # 2 required colons
    \s*                                                  # 0 or more whitespace characters
    (?P<signal_name>[\w-]*\w)                            # signal name
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<fields>.*?)                                      # annotations + description
    \s*                                                  # 0 or more whitespace characters
    :?                                                   # invalid delimiter
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching action identifiers.
ACTION_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<class_name>[\w]+)                                # class name
    \s*                                                  # 0 or more whitespace characters
    \|{1}                                                # 1 required vertical bar
    \s*                                                  # 0 or more whitespace characters
    (?P<action_name>[\w-]+\.[\w-]+)                      # action name
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<fields>.*?)                                      # annotations + description
    \s*                                                  # 0 or more whitespace characters
    :?                                                   # invalid delimiter
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching struct fields.
FIELD_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<class_name>[\w]+)                                # class name
    \s*                                                  # 0 or more whitespace characters
    \.{1}                                                # 1 required dot
    \s*                                                  # 0 or more whitespace characters
    (?P<field_name>[\w-]*\w)                             # field name
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<fields>.*?)                                      # annotations + description
    \s*                                                  # 0 or more whitespace characters
    :?                                                   # invalid delimiter
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching parameters.
PARAMETER_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    @                                                    # @ character
    (?P<parameter_name>[\w-]*\w|.*?\.\.\.)               # parameter name
    \s*                                                  # 0 or more whitespace characters
    :{1}                                                 # 1 required delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<fields>.*?)                                      # annotations + description
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching tags.
_all_tags = '|'.join(ALL_TAGS).replace(' ', r'\s')
TAG_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<tag_name>''' + _all_tags + r''')                 # tag name
    \s*                                                  # 0 or more whitespace characters
    :{1}                                                 # 1 required delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<fields>.*?)                                      # annotations + value + description
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE | re.IGNORECASE)

# Pattern matching value and description fields for TAG_DEPRECATED & TAG_SINCE tags.
TAG_VALUE_VERSION_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<value>([0-9\.])*)                                # value
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<description>.*?)                                 # description
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE)

# Pattern matching value and description fields for TAG_STABILITY tags.
TAG_VALUE_STABILITY_RE = re.compile(
    r'''
    ^                                                    # start
    \s*                                                  # 0 or more whitespace characters
    (?P<value>(stable|unstable|private|internal)?)       # value
    \s*                                                  # 0 or more whitespace characters
    (?P<delimiter>:?)                                    # delimiter
    \s*                                                  # 0 or more whitespace characters
    (?P<description>.*?)                                 # description
    \s*                                                  # 0 or more whitespace characters
    $                                                    # end
    ''',
    re.UNICODE | re.VERBOSE | re.IGNORECASE)


class GtkDocAnnotations(OrderedDict):
    '''
    An ordered dictionary mapping annotation names to annotation options (if any). Annotation
    options can be either a :class:`list`, a :class:`giscanner.collections.OrderedDict`
    (depending on the annotation name)or :const:`None`.
    '''

    __slots__ = ('position')

    def __init__(self, *args, **kwargs):
        #: A :class:`giscanner.message.Position` instance specifying the location of the
        #: annotations in the source file or :const:`None`.
        self.position = kwargs.pop('position', None)

        OrderedDict.__init__(self, *args, **kwargs)

    def __copy__(self):
        return GtkDocAnnotations(self, position=self.position)


class GtkDocAnnotatable(object):
    '''
    Base class for GTK-Doc comment block parts that can be annotated.
    '''

    __slots__ = ('position', 'annotations')

    #: A :class:`tuple` of annotation name constants that are valid for this object. Annotation
    #: names not in this :class:`tuple` will be reported as *unknown* by :func:`validate`. The
    #: :attr:`valid_annotations` class attribute should be overridden by subclasses.
    valid_annotations = ()  # type: Tuple[str,...]

    def __init__(self, position=None):
        #: A :class:`giscanner.message.Position` instance specifying the location of the
        #: annotatable comment block part in the source file or :const:`None`.
        self.position = position

        #: A :class:`GtkDocAnnotations` instance representing the annotations
        #: applied to this :class:`GtkDocAnnotatable` instance.
        self.annotations = GtkDocAnnotations()

    def __repr__(self):
        return "<GtkDocAnnotatable '%s'>" % (self.annotations, )

    def validate(self):
        '''
        Validate annotations stored by the :class:`GtkDocAnnotatable` instance, if any.
        '''

        if self.annotations:
            position = self.annotations.position

            for ann_name, options in self.annotations.items():
                if ann_name in self.valid_annotations:
                    validate = getattr(self, '_do_validate_' + ann_name.replace('-', '_'))
                    validate(position, ann_name, options)
                elif ann_name in ALL_ANNOTATIONS:
                    # Not error() as ann_name might be valid in some newer
                    # GObject-Instrospection version.
                    warn('unexpected annotation: %s' % (ann_name, ), position)
                else:
                    # Not error() as ann_name might be valid in some newer
                    # GObject-Instrospection version.
                    warn('unknown annotation: %s' % (ann_name, ), position)

                # Validate that (nullable) and (not nullable) are not both
                # present. Same for (allow-none) and (not nullable).
                if ann_name == ANN_NOT and OPT_NOT_NULLABLE in options:
                    if ANN_NULLABLE in self.annotations:
                        warn('cannot have both "%s" and "%s" present' %
                             (ANN_NOT + ' ' + OPT_NOT_NULLABLE, ANN_NULLABLE),
                             position)
                    if ANN_ALLOW_NONE in self.annotations:
                        warn('cannot have both "%s" and "%s" present' %
                             (ANN_NOT + ' ' + OPT_NOT_NULLABLE, ANN_ALLOW_NONE),
                             position)

                # Similarly for (optional) and (not optional).
                if ann_name == ANN_NOT and OPT_NOT_OPTIONAL in options:
                    if ANN_OPTIONAL in self.annotations:
                        warn('cannot have both "%s" and "%s" present' %
                             (ANN_NOT + ' ' + OPT_NOT_OPTIONAL, ANN_OPTIONAL),
                             position)

    def _validate_options(self, position, ann_name, n_options, expected_n_options, operator,
                          message):
        '''
        Validate the number of options held by an annotation according to the test
        ``operator(n_options, expected_n_options)``.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param n_options: number of options held by the annotation
        :param expected_n_options: number of expected options
        :param operator: an operator function from python's :mod:`operator` module, for example
                         :func:`operator.ne` or :func:`operator.lt`
        :param message: warning message used when the test
                        ``operator(n_options, expected_n_options)`` fails.
        '''

        if n_options == 0:
            t = 'none'
        else:
            t = '%d' % (n_options, )

        if expected_n_options == 0:
            s = 'no options'
        elif expected_n_options == 1:
            s = 'one option'
        else:
            s = '%d options' % (expected_n_options, )

        if operator(n_options, expected_n_options):
            warn('"%s" annotation %s %s, %s given' % (ann_name, message, s, t), position)

    def _validate_annotation(self, position, ann_name, options, choices=None,
                             exact_n_options=None, min_n_options=None, max_n_options=None):
        '''
        Validate an annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to be validated
        :param choices: an iterable of allowed option names or :const:`None` to skip this test
        :param exact_n_options: exact number of expected options or :const:`None` to skip this test
        :param min_n_options: minimum number of expected options or :const:`None` to skip this test
        :param max_n_options: maximum number of expected options or :const:`None` to skip this test
        '''

        n_options = len(options)

        if exact_n_options is not None:
            self._validate_options(position,
                                   ann_name, n_options, exact_n_options, ne, 'needs')

        if min_n_options is not None:
            self._validate_options(position,
                                   ann_name, n_options, min_n_options, lt, 'takes at least')

        if max_n_options is not None:
            self._validate_options(position,
                                   ann_name, n_options, max_n_options, gt, 'takes at most')

        if options and choices is not None:
            option = options[0]
            if option not in choices:
                warn('invalid "%s" annotation option: "%s"' % (ann_name, option), position)

    def _do_validate_allow_none(self, position, ann_name, options):
        '''
        Validate the ``(allow-none)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options held by the annotation
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_array(self, position, ann_name, options):
        '''
        Validate the ``(array)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options held by the annotation
        '''

        if len(options) == 0:
            return

        for option, value in options.items():
            if option == OPT_ARRAY_FIXED_SIZE:
                try:
                    int(value)
                except (TypeError, ValueError):
                    if value is None:
                        warn('"%s" annotation option "%s" needs a value' % (ann_name, option),
                             position)
                    else:
                        warn('invalid "%s" annotation option "%s" value "%s", must be an integer' %
                             (ann_name, option, value),
                             position)
            elif option == OPT_ARRAY_ZERO_TERMINATED:
                if value is not None and value not in ['0', '1']:
                    warn('invalid "%s" annotation option "%s" value "%s", must be 0 or 1' %
                         (ann_name, option, value),
                         position)
            elif option == OPT_ARRAY_LENGTH:
                if value is None:
                    warn('"%s" annotation option "length" needs a value' % (ann_name, ),
                         position)
            else:
                warn('invalid "%s" annotation option: "%s"' % (ann_name, option),
                     position)

    def _do_validate_async_func(self, position, ann_name, options):
        '''
        Validate the ``(async-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_attributes(self, position, ann_name, options):
        '''
        Validate the ``(attributes)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        # The 'attributes' annotation allows free form annotations.
        pass

    def _do_validate_closure(self, position, ann_name, options):
        '''
        Validate the ``(closure)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, max_n_options=1)

    def _do_validate_constructor(self, position, ann_name, options):
        '''
        Validate the ``(constructor)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_copy_func(self, position, ann_name, options):
        '''
        Validate the ``(copy-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_default_value(self, position, ann_name, options):
        '''
        Validate the ``(default-value)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        # The 'default-value' annotation allows free form annotations.
        pass

    def _do_validate_destroy(self, position, ann_name, options):
        '''
        Validate the ``(destroy)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_element_type(self, position, ann_name, options):
        '''
        Validate the ``(element)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, min_n_options=1, max_n_options=2)

    def _do_validate_emitter(self, position, ann_name, options):
        '''
        Validate the ``(emitter)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_finish_func(self, position, ann_name, options):
        '''
        Validate the ``(finish-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_foreign(self, position, ann_name, options):
        '''
        Validate the ``(foreign)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_free_func(self, position, ann_name, options):
        '''
        Validate the ``(free-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_get_property(self, position, ann_name, options):
        '''
        Validate the ``(get-property)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_get_value_func(self, position, ann_name, options):
        '''
        Validate the ``(value-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_getter(self, position, ann_name, options):
        '''
        Validate the ``(getter)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_in(self, position, ann_name, options):
        '''
        Validate the ``(in)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_inout(self, position, ann_name, options):
        '''
        Validate the ``(in-out)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_method(self, position, ann_name, options):
        '''
        Validate the ``(method)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_nullable(self, position, ann_name, options):
        '''
        Validate the ``(nullable)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options held by the annotation
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_optional(self, position, ann_name, options):
        '''
        Validate the ``(optional)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options held by the annotation
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_not(self, position, ann_name, options):
        '''
        Validate the ``(not)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options held by the annotation
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1,
                                  choices=NOT_OPTIONS)

    def _do_validate_out(self, position, ann_name, options):
        '''
        Validate the ``(out)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, max_n_options=1,
                                  choices=OUT_OPTIONS)

    def _do_validate_ref_func(self, position, ann_name, options):
        '''
        Validate the ``(ref-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_rename_to(self, position, ann_name, options):
        '''
        Validate the ``(rename-to)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_scope(self, position, ann_name, options):
        '''
        Validate the ``(scope)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1,
                                  choices=SCOPE_OPTIONS)

    def _do_validate_set_property(self, position, ann_name, options):
        '''
        Validate the ``(set-property)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_set_value_func(self, position, ann_name, options):
        '''
        Validate the ``(value-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_setter(self, position, ann_name, options):
        '''
        Validate the ``(setter)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_skip(self, position, ann_name, options):
        '''
        Validate the ``(skip)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=0)

    def _do_validate_sync_func(self, position, ann_name, options):
        '''
        Validate the ``(sync-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_transfer(self, position, ann_name, options):
        '''
        Validate the ``(transfer)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1,
                                  choices=TRANSFER_OPTIONS)

    def _do_validate_type(self, position, ann_name, options):
        '''
        Validate the ``(type)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_unref_func(self, position, ann_name, options):
        '''
        Validate the ``(unref-func)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_value(self, position, ann_name, options):
        '''
        Validate the ``(value)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)

    def _do_validate_virtual(self, position, ann_name, options):
        '''
        Validate the ``(virtual)`` annotation.

        :param position: :class:`giscanner.message.Position` of the line in the source file
                         containing the annotation to be validated
        :param ann_name: name of the annotation holding the options to validate
        :param options: annotation options to validate
        '''

        self._validate_annotation(position, ann_name, options, exact_n_options=1)


class GtkDocParameter(GtkDocAnnotatable):
    '''
    Represents a GTK-Doc parameter part.
    '''

    __slots__ = ('name', 'description')

    valid_annotations = (
        ANN_ALLOW_NONE,
        ANN_ARRAY,
        ANN_ATTRIBUTES,
        ANN_CLOSURE,
        ANN_DESTROY,
        ANN_ELEMENT_TYPE,
        ANN_IN,
        ANN_INOUT,
        ANN_OUT,
        ANN_SCOPE,
        ANN_SKIP,
        ANN_TRANSFER,
        ANN_TYPE,
        ANN_OPTIONAL,
        ANN_NULLABLE,
        ANN_NOT,
    )

    def __init__(self, name, position=None):
        GtkDocAnnotatable.__init__(self, position)

        #: Parameter name.
        self.name = name

        #: Parameter description or :const:`None`.
        self.description = None

    def __repr__(self):
        return "<GtkDocParameter '%s' %r>" % (self.name, self.annotations)


class GtkDocTag(GtkDocAnnotatable):
    '''
    Represents a GTK-Doc tag part.
    '''

    __slots__ = ('name', 'value', 'description')

    valid_annotations = (
        ANN_ALLOW_NONE,
        ANN_ARRAY,
        ANN_ATTRIBUTES,
        ANN_ELEMENT_TYPE,
        ANN_SKIP,
        ANN_TRANSFER,
        ANN_TYPE,
        ANN_NULLABLE,
        ANN_OPTIONAL,
        ANN_NOT,
    )

    def __init__(self, name, position=None):
        GtkDocAnnotatable.__init__(self, position)

        #: Tag name.
        self.name = name

        #: Tag value or :const:`None`.
        self.value = None

        #: Tag description or :const:`None`.
        self.description = None

    def __repr__(self):
        return "<GtkDocTag '%s' %r>" % (self.name, self.annotations)


class GtkDocCommentBlock(GtkDocAnnotatable):
    '''
    Represents a GTK-Doc comment block.
    '''

    __slots__ = ('code_before', 'code_after', 'indentation',
                 'name', 'params', 'description', 'tags')

    #: Valid annotation names for the GTK-Doc comment block identifier part.
    valid_annotations = (
        ANN_ATTRIBUTES,
        ANN_ASYNC_FUNC,
        ANN_CONSTRUCTOR,
        ANN_COPY_FUNC,
        ANN_DEFAULT_VALUE,
        ANN_EMITTER,
        ANN_FINISH_FUNC,
        ANN_FOREIGN,
        ANN_FREE_FUNC,
        ANN_GET_PROPERTY,
        ANN_GET_VALUE_FUNC,
        ANN_GETTER,
        ANN_METHOD,
        ANN_REF_FUNC,
        ANN_RENAME_TO,
        ANN_SET_PROPERTY,
        ANN_SET_VALUE_FUNC,
        ANN_SETTER,
        ANN_SKIP,
        ANN_SYNC_FUNC,
        ANN_TRANSFER,
        ANN_TYPE,
        ANN_UNREF_FUNC,
        ANN_VALUE,
        ANN_VFUNC,
    )

    def __init__(self, name, position=None):
        GtkDocAnnotatable.__init__(self, position)

        #: Code preceding the GTK-Doc comment block start token ("``/**``"), if any.
        self.code_before = None

        #: Code following the GTK-Doc comment block end token ("``*/``"), if any.
        self.code_after = None

        #: List of indentation levels (preceding the "``*``") for all lines in the comment
        #: block's source text.
        self.indentation = []

        #: Identifier name.
        self.name = name

        #: Ordered dictionary mapping parameter names to :class:`GtkDocParameter` instances
        #: applied to this :class:`GtkDocCommentBlock`.
        self.params = OrderedDict()

        #: The GTK-Doc comment block description part.
        self.description = None

        #: Ordered dictionary mapping tag names to :class:`GtkDocTag` instances
        #: applied to this :class:`GtkDocCommentBlock`.
        self.tags = OrderedDict()

    def _compare(self, other, op):
        # Note: This is used by g-ir-annotation-tool, which does a ``sorted(blocks.values())``,
        #       meaning that keeping this around makes update-glib-annotations.py patches
        #       easier to review.
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
        return "<GtkDocCommentBlock '%s' %r>" % (self.name, self.annotations)

    def validate(self):
        '''
        Validate annotations applied to the :class:`GtkDocCommentBlock` identifier, parameters
        and tags.
        '''
        GtkDocAnnotatable.validate(self)

        for param in self.params.values():
            param.validate()

        for tag in self.tags.values():
            tag.validate()


#: Result object returned by :class:`GtkDocCommentBlockParser`._parse_annotations()
_ParseAnnotationsResult = namedtuple(
    '_ParseAnnotationsResult',
    ['success', 'annotations', 'annotations_changed',
     'start_pos', 'end_pos'])

#: Result object returned by :class:`GtkDocCommentBlockParser`._parse_fields()
_ParseFieldsResult = namedtuple(
    '_ParseFieldsResult',
    ['success', 'annotations', 'annotations_changed',
     'description'])


class GtkDocCommentBlockParser(object):
    '''
    Parse GTK-Doc comment blocks into a parse tree built out of :class:`GtkDocCommentBlock`,
    :class:`GtkDocParameter`, :class:`GtkDocTag` and :class:`GtkDocAnnotations`
    objects. This parser tries to accept malformed input whenever possible and does
    not cause the process to exit on syntax errors. It does however emit:

        * warning messages at the slightest indication of recoverable malformed input and
        * error messages for unrecoverable malformed input

    whenever possible. Recoverable, in this context, means that we can serialize the
    :class:`GtkDocCommentBlock` instance using a :class:`GtkDocCommentBlockWriter` without
    information being lost. It is usually a good idea to heed these warning and error messages
    as malformed input can result in both:

        * invalid GTK-Doc output (HTML, pdf, ...) when the comment blocks are parsed
          with GTK-Doc's gtkdoc-mkdb
        * unexpected introspection behavior, for example missing parameters in the
          generated .gir and .typelib files

    .. NOTE:: :class:`GtkDocCommentBlockParser` functionality is based on gtkdoc-mkdb's
        `ScanSourceFile()`_ function.

    .. _ScanSourceFile():
           http://git.gnome.org/browse/gtk-doc/tree/gtkdoc-mkdb.in#n3722
    '''

    def parse_comment_blocks(self, comments):
        '''
        Parse multiple GTK-Doc comment blocks.

        :param comments: an iterable of ``(comment, filename, lineno)`` tuples
        :returns: a dictionary mapping identifier names to :class:`GtkDocCommentBlock` objects
        '''

        comment_blocks = {}

        for (comment, filename, lineno) in comments:
            try:
                comment_block = self.parse_comment_block(comment, filename, lineno)
            except Exception as e:
                error('unrecoverable parse error, please file a GObject-Introspection bug'
                      'report including the complete comment block at the indicated location. %s' %
                      str(e),
                      Position(filename, lineno))
                continue

            if comment_block is not None:
                # Note: previous versions of this parser did not check if an identifier was
                #       already stored in comment_blocks, so when different comment blocks where
                #       encountered documenting the same identifier the last comment block seen
                #       "wins". Keep this behavior for backwards compatibility, but emit a warning.
                if comment_block.name in comment_blocks:
                    firstseen = comment_blocks[comment_block.name]
                    path = os.path.dirname(firstseen.position.filename)
                    warn('multiple comment blocks documenting \'%s:\' identifier '
                         '(already seen at %s).' %
                         (comment_block.name, firstseen.position.format(path)),
                         comment_block.position)

                comment_blocks[comment_block.name] = comment_block

        return comment_blocks

    def parse_comment_block(self, comment, filename, lineno):
        '''
        Parse a single GTK-Doc comment block.

        :param comment: string representing the GTK-Doc comment block including it's
                        start ("``/**``") and end ("``*/``") tokens.
        :param filename: source file name where the comment block originated from
        :param lineno: line number in the source file where the comment block starts
        :returns: a :class:`GtkDocCommentBlock` object or ``None``
        '''

        code_before = ''
        code_after = ''
        comment_block_pos = Position(filename, lineno)
        comment_lines = re.sub(LINE_BREAK_RE, '\n', comment).split('\n')
        comment_lines_len = len(comment_lines)

        # Check for the start of the comment block.
        result = COMMENT_BLOCK_START_RE.match(comment_lines[0])
        if result:
            # Skip single line comment blocks
            if comment_lines_len == 1:
                position = Position(filename, lineno)
                error('Skipping invalid GTK-Doc comment block:',
                     position, None, result.end('code'), comment_lines[0])
                return None

            code_before = result.group('code')
            comment = result.group('comment')

            if code_before:
                position = Position(filename, lineno)
                warn('GTK-Doc comment block start token "/**" should not be preceded by code:',
                     position, None, result.end('code'), comment_lines[0])

            if comment:
                position = Position(filename, lineno)
                warn('GTK-Doc comment block start token "/**" should '
                     'not be followed by comment text:',
                     position, None, result.start('comment'), comment_lines[0])

                comment_lines[0] = comment
            else:
                del comment_lines[0]
        else:
            # Not a GTK-Doc comment block.
            return None

        # Check for the end of the comment block.
        result = COMMENT_BLOCK_END_RE.match(comment_lines[-1])
        if result:
            code_after = result.group('code')
            comment = result.group('comment')
            if code_after:
                position = Position(filename, lineno + comment_lines_len - 1)
                warn('GTK-Doc comment block end token "*/" should '
                     'not be followed by code:',
                     position, None, result.end('code'), comment_lines[-1])

            if comment:
                position = Position(filename, lineno + comment_lines_len - 1)
                warn('GTK-Doc comment block end token "*/" should '
                     'not be preceded by comment text:',
                     position, None, result.end('comment'), comment_lines[-1])

                comment_lines[-1] = comment
            else:
                del comment_lines[-1]
        else:
            # Not a GTK-Doc comment block.
            return None

        # If we get this far, we must be inside something
        # that looks like a GTK-Doc comment block.
        comment_block = None
        identifier_warned = False
        block_indent = []
        line_indent = None
        part_indent = None
        in_part = None
        current_part = None
        returns_seen = False

        for line in comment_lines:
            lineno += 1
            position = Position(filename, lineno)

            # Store the original line (without \n) and column offset
            # so we can generate meaningful warnings later on.
            original_line = line
            column_offset = 0

            # Store indentation level of the comment (before the ' * ')
            result = INDENTATION_RE.match(line)
            block_indent.append(result.group('indentation'))

            # Get rid of the ' * ' at the start of the line.
            result = COMMENT_ASTERISK_RE.match(line)
            if result:
                comment = result.group('comment')
                if comment:
                    error('invalid comment text:',
                          position, None, result.start('comment'), original_line)

                column_offset = result.end(0)
                line = line[result.end(0):]

            # Store indentation level of the line (after the ' * ').
            result = INDENTATION_RE.match(line)
            line_indent = len(result.group('indentation').replace('\t', '  '))

            ####################################################################
            # Check for GTK-Doc comment block identifier.
            ####################################################################
            if comment_block is None:
                result = SECTION_RE.match(line)

                if result:
                    identifier_name = 'SECTION:%s' % (result.group('section_name'), )
                    identifier_delimiter = None
                    identifier_fields = None
                    identifier_fields_start = None
                else:
                    result = PROPERTY_RE.match(line)

                    if result:
                        identifier_name = '%s:%s' % (result.group('class_name'),
                                                     result.group('property_name'))
                        identifier_delimiter = result.group('delimiter')
                        identifier_fields = result.group('fields')
                        identifier_fields_start = result.start('fields')
                    else:
                        result = SIGNAL_RE.match(line)

                        if result:
                            identifier_name = '%s::%s' % (result.group('class_name'),
                                                          result.group('signal_name'))
                            identifier_delimiter = result.group('delimiter')
                            identifier_fields = result.group('fields')
                            identifier_fields_start = result.start('fields')
                        else:
                            result = ACTION_RE.match(line)

                            if result:
                                identifier_name = 'ACTION:%s:%s' % (result.group('class_name'),
                                                                        result.group('action_name'))
                                identifier_delimiter = None
                                identifier_fields = None
                                identifier_fields_start = None
                            else:
                                result = FIELD_RE.match(line)

                                if result:
                                    identifier_name = '%s.%s' % (result.group('class_name'),
                                                                  result.group('field_name'))
                                    identifier_delimiter = result.group('delimiter')
                                    identifier_fields = result.group('fields')
                                    identifier_fields_start = result.start('fields')
                                else:
                                    result = SYMBOL_RE.match(line)

                                    if result:
                                        identifier_name = '%s' % (result.group('symbol_name'), )
                                        identifier_delimiter = result.group('delimiter')
                                        identifier_fields = result.group('fields')
                                        identifier_fields_start = result.start('fields')

                if result:
                    in_part = PART_IDENTIFIER
                    part_indent = line_indent

                    comment_block = GtkDocCommentBlock(identifier_name, comment_block_pos)
                    comment_block.code_before = code_before
                    comment_block.code_after = code_after

                    if identifier_fields:
                        res = self._parse_annotations(position,
                                                      column_offset + identifier_fields_start,
                                                      original_line,
                                                      identifier_fields)

                        if res.success:
                            if identifier_fields[res.end_pos:].strip():
                                # Not an identifier due to invalid trailing description field
                                result = None
                                in_part = None
                                part_indent = None
                                comment_block = None
                            else:
                                comment_block.annotations = res.annotations

                                if not identifier_delimiter and res.annotations:
                                    marker_pos = column_offset + result.start('delimiter')
                                    warn('missing ":" at column %s:' % (marker_pos + 1, ),
                                         position, None, marker_pos, original_line)

                if not result:
                    # Emit a single warning when the identifier is not found on the first line
                    if not identifier_warned:
                        identifier_warned = True
                        error('identifier not found on the first line:',
                              position, None, column_offset, original_line)
                continue

            ####################################################################
            # Check for comment block parameters.
            ####################################################################
            result = PARAMETER_RE.match(line)
            if result:
                part_indent = line_indent
                param_name = result.group('parameter_name')
                param_name_lower = param_name.lower()
                param_fields = result.group('fields')
                param_fields_start = result.start('fields')
                marker_pos = result.start('parameter_name') + column_offset

                if in_part not in [PART_IDENTIFIER, PART_PARAMETERS]:
                    warn('"@%s" parameter unexpected at this location:' % (param_name, ),
                         position, None, marker_pos, original_line)

                in_part = PART_PARAMETERS

                if param_name_lower == TAG_RETURNS:
                    # Deprecated return value as parameter instead of tag
                    param_name = TAG_RETURNS

                    if not returns_seen:
                        returns_seen = True
                    else:
                        error('encountered multiple "Returns" parameters or tags for "%s".' %
                              (comment_block.name, ),
                              position)

                    tag = GtkDocTag(TAG_RETURNS, position)

                    if param_fields:
                        result = self._parse_fields(position,
                                                    column_offset + param_fields_start,
                                                    original_line,
                                                    param_fields)
                        if result.success:
                            tag.annotations = result.annotations
                            tag.description = result.description
                    comment_block.tags[TAG_RETURNS] = tag
                    current_part = tag
                    continue
                elif (param_name == 'Varargs'
                or (param_name.endswith('...') and param_name != '...')):
                    # Deprecated @Varargs notation or named __VA_ARGS__ instead of @...
                    warn('"@%s" parameter is deprecated, please use "@..." instead:' %
                         (param_name, ),
                         position, None, marker_pos, original_line)
                    param_name = '...'

                if param_name in comment_block.params.keys():
                    error('multiple "@%s" parameters for identifier "%s":' %
                          (param_name, comment_block.name),
                          position, None, marker_pos, original_line)

                parameter = GtkDocParameter(param_name, position)

                if param_fields:
                    result = self._parse_fields(position,
                                                column_offset + param_fields_start,
                                                original_line,
                                                param_fields)
                    if result.success:
                        parameter.annotations = result.annotations
                        parameter.description = result.description

                comment_block.params[param_name] = parameter
                current_part = parameter
                continue

            ####################################################################
            # Check for comment block description.
            #
            # When we are parsing parameter parts or the identifier part (when
            # there are no parameters) and encounter an empty line, we must be
            # parsing the comment block description.
            #
            # Note: it is unclear why GTK-Doc does not allow paragraph breaks
            #       at this location as those might be handy describing
            #       parameters from time to time...
            ####################################################################
            if (EMPTY_LINE_RE.match(line) and in_part in [PART_IDENTIFIER, PART_PARAMETERS]):
                in_part = PART_DESCRIPTION
                part_indent = line_indent
                continue

            ####################################################################
            # Check for GTK-Doc comment block tags.
            ####################################################################
            result = TAG_RE.match(line)
            if result and line_indent <= part_indent:
                part_indent = line_indent
                tag_name = result.group('tag_name')
                tag_name_lower = tag_name.lower()
                tag_fields = result.group('fields')
                tag_fields_start = result.start('fields')
                marker_pos = result.start('tag_name') + column_offset

                if tag_name_lower in DEPRECATED_GI_ANN_TAGS:
                    # Deprecated GObject-Introspection specific tags.
                    # Emit a warning and transform these into annotations on the identifier
                    # instead, as agreed upon in http://bugzilla.gnome.org/show_bug.cgi?id=676133
                    warn('GObject-Introspection specific GTK-Doc tag "%s" '
                         'has been deprecated, please use annotations on the identifier '
                         'instead:' % (tag_name, ),
                         position, None, marker_pos, original_line)

                    # Translate deprecated tag name into corresponding annotation name
                    ann_name = tag_name_lower.replace(' ', '-')

                    if tag_name_lower == TAG_ATTRIBUTES:
                        transformed = ''
                        result = self._parse_fields(position,
                                                    result.start('tag_name') + column_offset,
                                                    line,
                                                    tag_fields.strip(),
                                                    None,
                                                    False,
                                                    False)

                        if result.success:
                            for annotation in result.annotations:
                                ann_options = self._parse_annotation_options_list(position,
                                                                                  marker_pos,
                                                                                  line,
                                                                                  annotation)
                                n_options = len(ann_options)
                                if n_options == 1:
                                    transformed = '%s %s' % (transformed, ann_options[0], )
                                elif n_options == 2:
                                    transformed = '%s %s=%s' % (transformed, ann_options[0],
                                                                ann_options[1])
                                else:
                                    # Malformed Attributes: tag
                                    error('malformed "Attributes:" tag will be ignored:',
                                          position, None, marker_pos, original_line)
                                    transformed = None

                            if transformed:
                                transformed = '%s %s' % (ann_name, transformed.strip())
                                ann_name, docannotation = self._parse_annotation(
                                    position,
                                    column_offset + tag_fields_start,
                                    original_line,
                                    transformed)
                                stored_annotation = comment_block.annotations.get('attributes')
                                if stored_annotation:
                                    error('Duplicate "Attributes:" annotation will '
                                          'be ignored:',
                                          position, None, marker_pos, original_line)
                                else:
                                    comment_block.annotations[ann_name] = docannotation
                    else:
                        ann_name, options = self._parse_annotation(position,
                                                               column_offset + tag_fields_start,
                                                               line,
                                                               '%s %s' % (ann_name, tag_fields))
                        comment_block.annotations[ann_name] = options

                    continue
                elif tag_name_lower == TAG_DESCRIPTION:
                    # Deprecated GTK-Doc Description: tag
                    warn('GTK-Doc tag "Description:" has been deprecated:',
                         position, None, marker_pos, original_line)

                    in_part = PART_DESCRIPTION

                    if comment_block.description is None:
                        comment_block.description = tag_fields
                    else:
                        comment_block.description += '\n%s' % (tag_fields, )
                    continue

                # Now that the deprecated stuff is out of the way, continue parsing real tags
                if (in_part == PART_DESCRIPTION
                or (in_part == PART_PARAMETERS and not comment_block.description)
                or (in_part == PART_IDENTIFIER and not comment_block.params and not
                comment_block.description)):
                    in_part = PART_TAGS

                if in_part != PART_TAGS:
                    in_part = PART_TAGS
                    warn('"%s:" tag unexpected at this location:' % (tag_name, ),
                         position, None, marker_pos, original_line)

                if tag_name_lower in [TAG_RETURN, TAG_RETURNS,
                                      TAG_RETURN_VALUE, TAG_RETURNS_VALUE]:
                    if not returns_seen:
                        returns_seen = True
                    else:
                        error('encountered multiple return value parameters or tags for "%s".' %
                              (comment_block.name, ),
                              position)

                    tag = GtkDocTag(TAG_RETURNS, position)

                    if tag_fields:
                        result = self._parse_fields(position,
                                                    column_offset + tag_fields_start,
                                                    original_line,
                                                    tag_fields)
                        if result.success:
                            tag.annotations = result.annotations
                            tag.description = result.description

                    comment_block.tags[TAG_RETURNS] = tag
                    current_part = tag
                    continue
                else:
                    if tag_name_lower in comment_block.tags.keys():
                        error('multiple "%s:" tags for identifier "%s":' %
                              (tag_name, comment_block.name),
                              position, None, marker_pos, original_line)

                    tag = GtkDocTag(tag_name_lower, position)

                    if tag_fields:
                        result = self._parse_fields(position,
                                                    column_offset + tag_fields_start,
                                                    original_line,
                                                    tag_fields)
                        if result.success:
                            if result.annotations:
                                error('annotations not supported for tag "%s:".' % (tag_name, ),
                                      position)

                            if tag_name_lower in [TAG_DEPRECATED, TAG_SINCE]:
                                result = TAG_VALUE_VERSION_RE.match(result.description)
                                tag.value = result.group('value')
                                tag.description = result.group('description')
                            elif tag_name_lower == TAG_STABILITY:
                                result = TAG_VALUE_STABILITY_RE.match(result.description)
                                tag.value = result.group('value').capitalize()
                                tag.description = result.group('description')

                    comment_block.tags[tag_name_lower] = tag
                    current_part = tag
                    continue

            ####################################################################
            # If we get here, we must be in the middle of a multiline
            # comment block, parameter or tag description.
            ####################################################################
            if EMPTY_LINE_RE.match(line) is None:
                line = line.rstrip()

            if in_part in [PART_IDENTIFIER, PART_DESCRIPTION]:
                if not comment_block.description:
                    if in_part == PART_IDENTIFIER:
                        r = self._parse_annotations(position, column_offset, original_line, line,
                                                    comment_block.annotations)

                        if r.success and r.annotations_changed:
                            comment_block.annotations = r.annotations
                            continue
                if comment_block.description is None:
                    comment_block.description = line
                else:
                    comment_block.description += '\n' + line
                continue
            elif in_part in [PART_PARAMETERS, PART_TAGS]:
                if not current_part.description:
                    r = self._parse_fields(position, column_offset, original_line, line,
                                           current_part.annotations)
                    if r.success and r.annotations_changed:
                        current_part.annotations = r.annotations
                        current_part.description = r.description
                        continue
                if current_part.description is None:
                    current_part.description = line
                else:
                    current_part.description += '\n' + line
                continue

        ########################################################################
        # Finished parsing this comment block.
        ########################################################################
        if comment_block:
            # We have picked up a couple of \n characters that where not
            # intended. Strip those.
            if comment_block.description:
                comment_block.description = comment_block.description.strip()

            for tag in comment_block.tags.values():
                self._clean_description_field(tag)

            for param in comment_block.params.values():
                self._clean_description_field(param)

            comment_block.indentation = block_indent
            comment_block.validate()
            return comment_block
        else:
            return None

    def _clean_description_field(self, part):
        '''
        Remove extraneous leading and trailing whitespace from description fields.

        :param part: a GTK-Doc comment block part having a description field
        '''

        if part.description:
            if part.description.strip() == '':
                part.description = None
            else:
                if EMPTY_LINE_RE.match(part.description.split('\n', 1)[0]):
                    part.description = part.description.rstrip()
                else:
                    part.description = part.description.strip()

    def _parse_annotation_options_list(self, position, column, line, options):
        '''
        Parse annotation options into a list. For example::

            ┌──────────────────────────────────────────────────────────────┐
            │ 'option1 option2 option3'                                    │ ─▷ source
            ├──────────────────────────────────────────────────────────────┤
            │ ['option1', 'option2', 'option3']                            │ ◁─ parsed options
            └──────────────────────────────────────────────────────────────┘

        :param position: :class:`giscanner.message.Position` of `line` in the source file
        :param column: start column of the `options` in the source file
        :param line: complete source line
        :param options: annotation options to parse
        :returns: a list of annotation options
        '''

        parsed = []

        if options:
            result = options.find('=')
            if result >= 0:
                warn('invalid annotation options: expected a "list" but '
                     'received "key=value pairs":',
                     position, None, column + result, line)
                parsed = self._parse_annotation_options_unknown(position, column, line, options)
            else:
                parsed = options.split(' ')

        return parsed

    def _parse_annotation_options_dict(self, position, column, line, options):
        '''
        Parse annotation options into a dict. For example::

            ┌──────────────────────────────────────────────────────────────┐
            │ 'option1=value1 option2 option3=value2'                      │ ─▷ source
            ├──────────────────────────────────────────────────────────────┤
            │ {'option1': 'value1', 'option2': None, 'option3': 'value2'}  │ ◁─ parsed options
            └──────────────────────────────────────────────────────────────┘

        :param position: :class:`giscanner.message.Position` of `line` in the source file
        :param column: start column of the `options` in the source file
        :param line: complete source line
        :param options: annotation options to parse
        :returns: an ordered dictionary of annotation options
        '''

        parsed = OrderedDict()

        if options:
            for p in options.split(' '):
                parts = p.split('=', 1)
                key = parts[0]
                value = parts[1] if len(parts) == 2 else None
                parsed[key] = value

        return parsed

    def _parse_annotation_options_unknown(self, position, column, line, options):
        '''
        Parse annotation options into a list holding a single item. This is used when the
        annotation options to parse in not known to be a list nor dict. For example::

            ┌──────────────────────────────────────────────────────────────┐
            │ '   option1 option2   option3=value1   '                     │ ─▷ source
            ├──────────────────────────────────────────────────────────────┤
            │ ['option1 option2   option3=value1']                         │ ◁─ parsed options
            └──────────────────────────────────────────────────────────────┘

        :param position: :class:`giscanner.message.Position` of `line` in the source file
        :param column: start column of the `options` in the source file
        :param line: complete source line
        :param options: annotation options to parse
        :returns: a list of annotation options
        '''

        if options:
            return [options.strip()]

    def _parse_annotation(self, position, column, line, annotation):
        '''
        Parse an annotation into the annotation name and a list or dict (depending on the
        name of the annotation) holding the options. For example::

            ┌──────────────────────────────────────────────────────────────┐
            │ 'name opt1=value1 opt2=value2 opt3'                          │ ─▷ source
            ├──────────────────────────────────────────────────────────────┤
            │ 'name', {'opt1': 'value1', 'opt2':'value2', 'opt3':None}     │ ◁─ parsed annotation
            └──────────────────────────────────────────────────────────────┘

            ┌──────────────────────────────────────────────────────────────┐
            │ 'name   opt1 opt2'                                           │ ─▷ source
            ├──────────────────────────────────────────────────────────────┤
            │ 'name', ['opt1', 'opt2']                                     │ ◁─ parsed annotation
            └──────────────────────────────────────────────────────────────┘

            ┌──────────────────────────────────────────────────────────────┐
            │ 'unkownname   unknown list of options'                       │ ─▷ source
            ├──────────────────────────────────────────────────────────────┤
            │ 'unkownname', ['unknown list of options']                    │ ◁─ parsed annotation
            └──────────────────────────────────────────────────────────────┘

        :param position: :class:`giscanner.message.Position` of `line` in the source file
        :param column: start column of the `annotation` in the source file
        :param line: complete source line
        :param annotation: annotation to parse
        :returns: a tuple containing the annotation name and options
        '''

        # Transform deprecated type syntax "tokens"
        annotation = annotation.replace('<', ANN_LPAR).replace('>', ANN_RPAR)

        parts = annotation.split(' ', 1)
        ann_name = parts[0].lower()
        ann_options = parts[1] if len(parts) == 2 else None

        if ann_name == ANN_INOUT_ALT:
            warn('"%s" annotation has been deprecated, please use "%s" instead:' %
                 (ANN_INOUT_ALT, ANN_INOUT),
                 position, None, column, line)

            ann_name = ANN_INOUT
        elif ann_name == ANN_ATTRIBUTE:
            warn('"%s" annotation has been deprecated, please use "%s" instead:' %
                 (ANN_ATTRIBUTE, ANN_ATTRIBUTES),
                 position, None, column, line)

            ann_name = ANN_ATTRIBUTES
            ann_options = self._parse_annotation_options_list(position, column, line, ann_options)
            n_options = len(ann_options)
            if n_options == 1:
                ann_options = ann_options[0]
            elif n_options == 2:
                ann_options = '%s=%s' % (ann_options[0], ann_options[1])
            else:
                error('malformed "(attribute)" annotation will be ignored:',
                      position, None, column, line)
                return None, None

        column += len(ann_name) + 2

        if ann_name in LIST_ANNOTATIONS:
            ann_options = self._parse_annotation_options_list(position, column, line, ann_options)
        elif ann_name in DICT_ANNOTATIONS:
            ann_options = self._parse_annotation_options_dict(position, column, line, ann_options)
        else:
            ann_options = self._parse_annotation_options_unknown(position, column, line,
                                                                 ann_options)

        return ann_name, ann_options

    def _parse_annotations(self, position, column, line, fields,
                           annotations=None, parse_options=True):
        '''
        Parse annotations into a :class:`GtkDocAnnotations` object.

        :param position: :class:`giscanner.message.Position` of `line` in the source file
        :param column: start column of the `annotations` in the source file
        :param line: complete source line
        :param fields: string containing the fields to parse
        :param annotations: a :class:`GtkDocAnnotations` object
        :param parse_options: whether options will be parsed into a :class:`GtkDocAnnotations`
                              object or into a :class:`list`
        :returns: if `parse_options` evaluates to True a :class:`GtkDocAnnotations` object,
                  a :class:`list` otherwise. If `line` does not contain any annotations,
                  :const:`None`
        '''

        if parse_options:
            if annotations is None:
                parsed_annotations = GtkDocAnnotations(position=position)
            else:
                parsed_annotations = annotations.copy()
        else:
            parsed_annotations = []

        parsed_annotations_changed = False

        i = 0
        parens_level = 0
        prev_char = ''
        char_buffer = []
        start_pos = 0
        end_pos = 0

        for i, cur_char in enumerate(fields):
            cur_char_is_space = cur_char.isspace()

            if cur_char == ANN_LPAR:
                parens_level += 1

                if parens_level == 1:
                    start_pos = i

                if prev_char == ANN_LPAR:
                    error('unexpected parentheses, annotations will be ignored:',
                          position, None, column + i, line)
                    return _ParseAnnotationsResult(False, None, None, None, None)
                elif parens_level > 1:
                    char_buffer.append(cur_char)
            elif cur_char == ANN_RPAR:
                parens_level -= 1

                if prev_char == ANN_LPAR:
                    error('unexpected parentheses, annotations will be ignored:',
                          position, None, column + i, line)
                    return _ParseAnnotationsResult(False, None, None, None, None)
                elif parens_level < 0:
                    error('unbalanced parentheses, annotations will be ignored:',
                          position, None, column + i, line)
                    return _ParseAnnotationsResult(False, None, None, None, None)
                elif parens_level == 0:
                    end_pos = i + 1

                    if parse_options is True:
                        name, options = self._parse_annotation(position,
                                                               column + start_pos,
                                                               line,
                                                               ''.join(char_buffer).strip())
                        if name is not None:
                            if name in parsed_annotations:
                                error('multiple "%s" annotations:' % (name, ),
                                      position, None, column + i, line)
                            parsed_annotations[name] = options
                            parsed_annotations_changed = True
                    else:
                        parsed_annotations.append(''.join(char_buffer).strip())
                        parsed_annotations_changed = True

                    char_buffer = []
                else:
                    char_buffer.append(cur_char)
            elif cur_char_is_space:
                if parens_level > 0:
                    char_buffer.append(cur_char)
            else:
                if parens_level == 0:
                    break
                else:
                    char_buffer.append(cur_char)

            prev_char = cur_char

        if parens_level > 0:
            error('unbalanced parentheses, annotations will be ignored:',
                  position, None, column + i, line)
            return _ParseAnnotationsResult(False, None, None, None, None)
        else:
            return _ParseAnnotationsResult(True, parsed_annotations, parsed_annotations_changed,
                                           start_pos, end_pos)

    def _parse_fields(self, position, column, line, fields, annotations=None,
                      parse_options=True, validate_description_field=True):
        '''
        Parse annotations out of field data. For example::

            ┌──────────────────────────────────────────────────────────────┐
            │ '(skip): description of some parameter'                      │ ─▷ source
            ├──────────────────────────────────────────────────────────────┤
            │ ({'skip': []}, 'description of some parameter')              │ ◁─ annotations and
            └──────────────────────────────────────────────────────────────┘    remaining fields

        :param position: :class:`giscanner.message.Position` of `line` in the source file
        :param column: start column of `fields` in the source file
        :param line: complete source line
        :param fields: string containing the fields to parse
        :param parse_options: whether options will be parsed into a :class:`GtkDocAnnotations`
                              object or into a :class:`list`
        :param validate_description_field: :const:`True` to validate the description field
        :returns: if `parse_options` evaluates to True a :class:`GtkDocAnnotations` object,
                  a :class:`list` otherwise. If `line` does not contain any annotations,
                  :const:`None` and a string holding the remaining fields
        '''
        description_field = ''
        result = self._parse_annotations(position, column, line, fields,
                                         annotations, parse_options)
        if result.success:
            description_field = fields[result.end_pos:].strip()

            if description_field and validate_description_field:
                if description_field.startswith(':'):
                    description_field = description_field[1:]
                else:
                    if result.end_pos > 0:
                        marker_pos = column + result.end_pos
                        warn('missing ":" at column %s:' % (marker_pos + 1, ),
                             position, None, marker_pos, line)

        return _ParseFieldsResult(result.success, result.annotations, result.annotations_changed,
                                  description_field)


class GtkDocCommentBlockWriter(object):
    '''
    Serialized :class:`GtkDocCommentBlock` objects into GTK-Doc comment blocks.
    '''

    def __init__(self, indent=True):
        #: :const:`True` if the original indentation preceding the "``*``" needs to be retained,
        #: :const:`False` otherwise. Default value is :const:`True`.
        self.indent = indent

    def _serialize_annotations(self, annotations):
        '''
        Serialize an annotation field. For example::

            ┌──────────────────────────────────────────────────────────────┐
            │ {'name': {'opt1': 'value1', 'opt2':'value2', 'opt3':None}    │ ◁─ GtkDocAnnotations
            ├──────────────────────────────────────────────────────────────┤
            │ '(name opt1=value1 opt2=value2 opt3)'                        │ ─▷ serialized
            └──────────────────────────────────────────────────────────────┘

            ┌──────────────────────────────────────────────────────────────┐
            │ {'name': ['opt1', 'opt2']}                                   │ ◁─ GtkDocAnnotations
            ├──────────────────────────────────────────────────────────────┤
            │ '(name opt1 opt2)'                                           │ ─▷ serialized
            └──────────────────────────────────────────────────────────────┘

            ┌──────────────────────────────────────────────────────────────┐
            │ {'unkownname': ['unknown list of options']}                  │ ◁─ GtkDocAnnotations
            ├──────────────────────────────────────────────────────────────┤
            │ '(unkownname unknown list of options)'                       │ ─▷ serialized
            └──────────────────────────────────────────────────────────────┘

        :param annotations: :class:`GtkDocAnnotations` to be serialized
        :returns: a string
        '''

        serialized = []

        for ann_name, options in annotations.items():
            if options:
                if isinstance(options, list):
                    serialize_options = ' '.join(options)
                else:
                    serialize_options = ''

                    for key, value in options.items():
                        if value:
                            serialize_options += '%s=%s ' % (key, value)
                        else:
                            serialize_options += '%s ' % (key, )

                    serialize_options = serialize_options.strip()

                serialized.append('(%s %s)' % (ann_name, serialize_options))
            else:
                serialized.append('(%s)' % (ann_name, ))

        return ' '.join(serialized)

    def _serialize_parameter(self, parameter):
        '''
        Serialize a parameter.

        :param parameter: :class:`GtkDocParameter` to be serialized
        :returns: a string
        '''

        # parameter_name field
        serialized = '@%s' % (parameter.name, )

        # annotations field
        if parameter.annotations:
            serialized += ': ' + self._serialize_annotations(parameter.annotations)

        # description field
        if parameter.description:
            if parameter.description.startswith('\n'):
                serialized += ':' + parameter.description
            else:
                serialized += ': ' + parameter.description
        else:
            serialized += ':'

        return serialized.split('\n')

    def _serialize_tag(self, tag):
        '''
        Serialize a tag.

        :param tag: :class:`GtkDocTag` to be serialized
        :returns: a string
        '''

        # tag_name field
        serialized = tag.name.capitalize()

        # annotations field
        if tag.annotations:
            serialized += ': ' + self._serialize_annotations(tag.annotations)

        # value field
        if tag.value:
            serialized += ': ' + tag.value

        # description field
        if tag.description:
            if tag.description.startswith('\n'):
                serialized += ':' + tag.description
            else:
                serialized += ': ' + tag.description

        if not tag.value and not tag.description:
            serialized += ':'

        return serialized.split('\n')

    def write(self, block):
        '''
        Serialize a :class:`GtkDocCommentBlock` object.

        :param block: :class:`GtkDocCommentBlock` to be serialized
        :returns: a string
        '''

        if block is None:
            return ''
        else:
            lines = []

            # Identifier part
            if block.name.startswith('SECTION') or block.name.startswith('ACTION'):
                lines.append(block.name)
            else:
                if block.annotations:
                    annotations = self._serialize_annotations(block.annotations)
                    lines.append('%s: %s' % (block.name, annotations))
                else:
                    # Note: this delimiter serves no purpose other than most people being used
                    #       to reading/writing it. It is completely legal to ommit this.
                    lines.append('%s:' % (block.name, ))

            # Parameter parts
            for param in block.params.values():
                lines.extend(self._serialize_parameter(param))

            # Comment block description part
            if block.description:
                lines.append('')
                for l in block.description.split('\n'):
                    lines.append(l)

            # Tag parts
            if block.tags:
                # Note: this empty line servers no purpose other than most people being used
                #       to reading/writing it. It is completely legal to ommit this.
                lines.append('')
                for tag in block.tags.values():
                    lines.extend(self._serialize_tag(tag))

            # Restore comment block indentation and *
            if self.indent:
                indent = Counter(block.indentation).most_common(1)[0][0] or ' '
                if indent.endswith('\t'):
                    start_indent = indent
                    line_indent = indent + ' '
                else:
                    start_indent = indent[:-1]
                    line_indent = indent
            else:
                start_indent = ''
                line_indent = ' '

            i = 0
            while i < len(lines):
                line = lines[i]
                if line:
                    lines[i] = '%s* %s\n' % (line_indent, line)
                else:
                    lines[i] = '%s*\n' % (line_indent, )
                i += 1

            # Restore comment block start and end tokens
            lines.insert(0, '%s/**\n' % (start_indent, ))
            lines.append('%s*/\n' % (line_indent, ))

            # Restore code before and after comment block start and end tokens
            if block.code_before:
                lines.insert(0, '%s\n' % (block.code_before, ))

            if block.code_after:
                lines.append('%s\n' % (block.code_after, ))

            return ''.join(lines)
