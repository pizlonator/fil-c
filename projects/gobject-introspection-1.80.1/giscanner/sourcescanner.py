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
import tempfile

from .message import Position
from .ccompiler import CCompiler
from .utils import have_debug_flag, dll_dirs


dlldirs = dll_dirs()
dlldirs.add_dll_dirs(['gio-2.0'])
from giscanner._giscanner import SourceScanner as CSourceScanner
dlldirs.cleanup_dll_dirs()

HEADER_EXTS = ['.h', '.hpp', '.hxx']
SOURCE_EXTS = ['.c', '.cpp', '.cc', '.cxx']
ALL_EXTS = SOURCE_EXTS + HEADER_EXTS

(CSYMBOL_TYPE_INVALID,
 CSYMBOL_TYPE_ELLIPSIS,
 CSYMBOL_TYPE_CONST,
 CSYMBOL_TYPE_OBJECT,
 CSYMBOL_TYPE_FUNCTION,
 CSYMBOL_TYPE_FUNCTION_MACRO,
 CSYMBOL_TYPE_STRUCT,
 CSYMBOL_TYPE_UNION,
 CSYMBOL_TYPE_ENUM,
 CSYMBOL_TYPE_TYPEDEF,
 CSYMBOL_TYPE_MEMBER) = range(11)

(CTYPE_INVALID,
 CTYPE_VOID,
 CTYPE_BASIC_TYPE,
 CTYPE_TYPEDEF,
 CTYPE_STRUCT,
 CTYPE_UNION,
 CTYPE_ENUM,
 CTYPE_POINTER,
 CTYPE_ARRAY,
 CTYPE_FUNCTION) = range(10)

STORAGE_CLASS_NONE = 0
STORAGE_CLASS_TYPEDEF = 1 << 1
STORAGE_CLASS_EXTERN = 1 << 2
STORAGE_CLASS_STATIC = 1 << 3
STORAGE_CLASS_AUTO = 1 << 4
STORAGE_CLASS_REGISTER = 1 << 5
STORAGE_CLASS_THREAD_LOCAL = 1 << 6

TYPE_QUALIFIER_NONE = 0
TYPE_QUALIFIER_CONST = 1 << 1
TYPE_QUALIFIER_RESTRICT = 1 << 2
TYPE_QUALIFIER_VOLATILE = 1 << 3
TYPE_QUALIFIER_EXTENSION = 1 << 4

FUNCTION_NONE = 0
FUNCTION_INLINE = 1 << 1

(UNARY_ADDRESS_OF,
 UNARY_POINTER_INDIRECTION,
 UNARY_PLUS,
 UNARY_MINUS,
 UNARY_BITWISE_COMPLEMENT,
 UNARY_LOGICAL_NEGATION) = range(6)


def symbol_type_name(symbol_type):
    return {
        CSYMBOL_TYPE_INVALID: 'invalid',
        CSYMBOL_TYPE_ELLIPSIS: 'ellipsis',
        CSYMBOL_TYPE_CONST: 'const',
        CSYMBOL_TYPE_OBJECT: 'object',
        CSYMBOL_TYPE_FUNCTION: 'function',
        CSYMBOL_TYPE_FUNCTION_MACRO: 'function_macro',
        CSYMBOL_TYPE_STRUCT: 'struct',
        CSYMBOL_TYPE_UNION: 'union',
        CSYMBOL_TYPE_ENUM: 'enum',
        CSYMBOL_TYPE_TYPEDEF: 'typedef',
        CSYMBOL_TYPE_MEMBER: 'member'}.get(symbol_type)


def ctype_name(ctype):
    return {
        CTYPE_INVALID: 'invalid',
        CTYPE_VOID: 'void',
        CTYPE_BASIC_TYPE: 'basic',
        CTYPE_TYPEDEF: 'typedef',
        CTYPE_STRUCT: 'struct',
        CTYPE_UNION: 'union',
        CTYPE_ENUM: 'enum',
        CTYPE_POINTER: 'pointer',
        CTYPE_ARRAY: 'array',
        CTYPE_FUNCTION: 'function'}.get(ctype)


class SourceType(object):
    __members__ = ['type', 'base_type', 'name', 'type_qualifier',
                   'child_list', 'is_bitfield', 'function_specifier']

    def __init__(self, scanner, stype):
        self._scanner = scanner
        self._stype = stype

    def __repr__(self):
        return "<%s type='%s' name='%s'>" % (
            self.__class__.__name__,
            ctype_name(self.type),
            self.name)

    @property
    def type(self):
        return self._stype.type

    @property
    def base_type(self):
        if self._stype.base_type is not None:
            return SourceType(self._scanner, self._stype.base_type)

    @property
    def name(self):
        return self._stype.name

    @property
    def type_qualifier(self):
        return self._stype.type_qualifier

    @property
    def child_list(self):
        for symbol in self._stype.child_list:
            if symbol is None:
                continue
            yield SourceSymbol(self._scanner, symbol)

    @property
    def is_bitfield(self):
        return self._stype.is_bitfield

    @property
    def function_specifier(self):
        return self._stype.function_specifier


class SourceSymbol(object):
    __members__ = ['const_int', 'const_double', 'const_string', 'const_boolean',
                   'ident', 'type', 'base_type']

    def __init__(self, scanner, symbol):
        self._scanner = scanner
        self._symbol = symbol

    def __repr__(self):
        src = self.source_filename
        if src:
            line = self.line
            if line:
                src += ":'%s'" % (line, )
        return "<%s type='%s' ident='%s' src='%s'>" % (
            self.__class__.__name__,
            symbol_type_name(self.type),
            self.ident,
            src)

    @property
    def const_int(self):
        return self._symbol.const_int

    @property
    def const_double(self):
        return self._symbol.const_double

    @property
    def const_string(self):
        return self._symbol.const_string

    @property
    def const_boolean(self):
        return self._symbol.const_boolean

    @property
    def ident(self):
        return self._symbol.ident

    @property
    def type(self):
        return self._symbol.type

    @property
    def base_type(self):
        if self._symbol.base_type is not None:
            return SourceType(self._scanner, self._symbol.base_type)

    @property
    def source_filename(self):
        return self._symbol.source_filename

    @property
    def line(self):
        return self._symbol.line

    @property
    def private(self):
        return self._symbol.private

    @property
    def position(self):
        return Position(self._symbol.source_filename,
                        self._symbol.line)


class SourceScanner(object):

    def __init__(self):
        self._scanner = CSourceScanner()
        self._filenames = []
        self._cpp_options = []
        self._compiler = None

    # Public API

    def set_cpp_options(self, includes, defines, undefines, cflags=[]):
        self._cpp_options.extend(cflags)
        for prefix, args in [('-I', [os.path.realpath(f) for f in includes]),
                             ('-D', defines),
                             ('-U', undefines)]:
            for arg in (args or []):
                opt = prefix + arg
                if opt not in self._cpp_options:
                    self._cpp_options.append(opt)

    def set_compiler(self, compiler):
        self._compiler = compiler

    def parse_files(self, filenames):
        for filename in filenames:
            # self._scanner expects file names to be canonicalized and symlinks to be resolved
            filename = os.path.realpath(filename)
            self._scanner.append_filename(filename)
            self._filenames.append(filename)

        headers = []
        for filename in self._filenames:
            if os.path.splitext(filename)[1] in SOURCE_EXTS:
                self._scanner.lex_filename(filename)
            else:
                headers.append(filename)

        self._parse(headers)

    def parse_macros(self, filenames):
        self._scanner.set_macro_scan(True)
        # self._scanner expects file names to be canonicalized and symlinks to be resolved
        self._scanner.parse_macros([os.path.realpath(f) for f in filenames])
        self._scanner.set_macro_scan(False)

    def get_symbols(self):
        for symbol in self._scanner.get_symbols():
            yield SourceSymbol(self._scanner, symbol)

    def get_comments(self):
        return self._scanner.get_comments()

    def get_errors(self):
        return self._scanner.get_errors()

    def dump(self):
        print('-' * 30)
        for symbol in self._scanner.get_symbols():
            print(symbol.ident, symbol.base_type.name, symbol.type)

    # Private

    def _parse(self, filenames):
        if not filenames:
            return

        defines = ['__GI_SCANNER__']
        undefs = []

        cc = CCompiler(compiler_name=self._compiler)

        tmp_fd_cpp, tmp_name_cpp = tempfile.mkstemp(prefix='g-ir-cpp-',
                                                    suffix='.c',
                                                    dir=os.getcwd())
        with os.fdopen(tmp_fd_cpp, 'wb') as fp_cpp:
            self._write_preprocess_src(fp_cpp, defines, undefs, filenames)

        tmpfile_basename = os.path.basename(os.path.splitext(tmp_name_cpp)[0])

        # Output file name of the preprocessor, only really used on non-MSVC,
        # so we want the name to match the output file name of the MSVC preprocessor
        tmpfile_output = tmpfile_basename + '.i'

        cc.preprocess(tmp_name_cpp,
                      tmpfile_output,
                      self._cpp_options)

        if not have_debug_flag('save-temps'):
            os.unlink(tmp_name_cpp)
        self._scanner.parse_file(tmpfile_output)
        if not have_debug_flag('save-temps'):
            os.unlink(tmpfile_output)

    def _write_preprocess_src(self, fp, defines, undefs, filenames):
        # Write to the temp file for feeding into the preprocessor
        for define in defines:
            fp.write(('#ifndef %s\n' % (define, )).encode())
            fp.write(('# define %s\n' % (define, )).encode())
            fp.write('#endif\n'.encode())
        for undef in undefs:
            fp.write(('#undef %s\n' % (undef, )).encode())
        for filename in filenames:
            fp.write(('#include <%s>\n' % (filename, )).encode())
