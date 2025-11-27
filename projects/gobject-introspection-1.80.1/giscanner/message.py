# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2010 Red Hat, Inc.
# Copyright (C) 2010 Johan Dahlin
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
import sys
import operator

from .utils import break_on_debug_flag

(WARNING,
 ERROR,
 FATAL) = range(3)


class Position(object):
    """
    Represents a position in the source file which we
    want to inform about.
    """

    __slots__ = ('filename', 'line', 'column', 'is_typedef')

    def __init__(self, filename=None, line=None, column=None, is_typedef=False):
        self.filename = filename
        self.line = line
        self.column = column
        self.is_typedef = is_typedef

    def _compare(self, other, op):
        return op((self.filename, self.line, self.column),
                  (other.filename, other.line, other.column))

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
        return hash((self.filename, self.line, self.column))

    def __repr__(self):
        return '<Position %s:%d:%d>' % (os.path.basename(self.filename),
                                        self.line or -1,
                                        self.column or -1)

    def format(self, cwd):
        # Windows: We may be using different drives self.filename and cwd,
        #          which leads to a ValuError when using os.path.relpath().
        #          In that case, just use the entire path of self.filename
        try:
            filename = os.path.relpath(os.path.realpath(self.filename),
                                       os.path.realpath(cwd))
        except ValueError:
            filename = os.path.realpath(self.filename)

        if self.column is not None:
            return '%s:%d:%d' % (filename, self.line, self.column)
        elif self.line is not None:
            return '%s:%d' % (filename, self.line, )
        else:
            return '%s:' % (filename, )


class MessageLogger(object):
    _instance = None

    def __init__(self, namespace=None, output=None):
        if output is None:
            output = sys.stderr
        self._cwd = os.getcwd()
        self._output = output
        self._namespace = namespace
        self._enable_warnings = False
        self._enable_strict = False
        self._warning_count = 0

    @classmethod
    def get(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = cls(*args, **kwargs)
        return cls._instance

    def enable_warnings(self, value):
        self._enable_warnings = bool(value)

    @property
    def warnings_enabled(self):
        return self._enable_warnings

    def enable_strict(self, value):
        self._enable_strict = bool(value)

    @property
    def strict_enabled(self):
        return self._enable_strict

    def get_warning_count(self):
        return self._warning_count

    def log(self, log_type, text, positions=None, prefix=None, marker_pos=None, marker_line=None):
        """
        Log a warning, using optional file positioning information.
        If the warning is related to a ast.Node type, see log_node().
        """
        break_on_debug_flag('warning')

        self._warning_count += 1

        if not self._enable_warnings and log_type in (WARNING, ERROR):
            return

        if isinstance(positions, set):
            positions = list(positions)
        if isinstance(positions, Position):
            positions = [positions]

        if not positions:
            positions = [Position('<unknown>')]

        for position in positions[:-1]:
            self._output.write("%s:\n" % (position.format(cwd=self._cwd), ))
        last_position = positions[-1].format(cwd=self._cwd)

        if log_type == WARNING:
            error_type = "Warning"
        elif log_type == ERROR:
            error_type = "Error"
        elif log_type == FATAL:
            error_type = "Fatal"

        if marker_pos is not None and marker_line is not None:
            text = '%s\n%s\n%s' % (text, marker_line, ' ' * marker_pos + '^')

        if prefix:
            if self._namespace:
                text = ('%s: %s: %s: %s: %s\n' % (last_position, error_type,
                                                  self._namespace.name, prefix, text))
            else:
                text = ('%s: %s: %s: %s\n' % (last_position, error_type,
                                              prefix, text))
        else:
            if self._namespace:
                text = ('%s: %s: %s: %s\n' % (last_position, error_type,
                                              self._namespace.name, text))
            else:
                text = ('%s: %s: %s\n' % (last_position, error_type, text))

        self._output.write(text)

        if log_type == FATAL:
            break_on_debug_flag('fatal')
            raise SystemExit(text)

    def log_node(self, log_type, node, text, context=None, positions=None):
        """
        Log a warning, using information about file positions from
        the given node.  The optional context argument, if given, should be
        another ast.Node type which will also be displayed.  If no file position
        information is available from the node, the position data from the
        context will be used.
        """
        if positions:
            pass
        elif getattr(node, 'file_positions', None):
            positions = node.file_positions
        elif context and context.file_positions:
            positions = context.file_positions
        else:
            positions = set()

        if context:
            text = "%s: %s" % (getattr(context, 'symbol', context.name), text)
        elif not positions and hasattr(node, 'name'):
            text = "(%s)%s: %s" % (node.__class__.__name__, node.name, text)

        self.log(log_type, text, positions)

    def log_symbol(self, log_type, symbol, text):
        """Log a warning in the context of the given symbol."""
        self.log(log_type, text, symbol.position,
                 prefix="symbol='%s'" % (symbol.ident, ))


def log_node(log_type, node, text, context=None, positions=None):
    ml = MessageLogger.get()
    ml.log_node(log_type, node, text, context=context, positions=positions)


def warn(text, positions=None, prefix=None, marker_pos=None, marker_line=None):
    ml = MessageLogger.get()
    ml.log(WARNING, text, positions, prefix, marker_pos, marker_line)


def warn_node(node, text, context=None, positions=None):
    log_node(WARNING, node, text, context=context, positions=positions)


def error_node(node, text, context=None, positions=None):
    log_node(ERROR, node, text, context=context, positions=positions)


def strict_node(node, text, context=None, positions=None):
    ml = MessageLogger.get()
    if ml.strict_enabled:
        ml.log_node(WARNING, node, text, context=context, positions=positions)


def warn_symbol(symbol, text):
    ml = MessageLogger.get()
    ml.log_symbol(WARNING, symbol, text)


def error(text, positions=None, prefix=None, marker_pos=None, marker_line=None):
    ml = MessageLogger.get()
    ml.log(ERROR, text, positions, prefix, marker_pos, marker_line)


def fatal(text, positions=None, prefix=None, marker_pos=None, marker_line=None):
    ml = MessageLogger.get()
    ml.log(FATAL, text, positions, prefix, marker_pos, marker_line)
