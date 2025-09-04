#!/usr/bin/python3
# ELF support functionality for Python.
# Copyright (C) 2022-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

"""Basic ELF parser.

Use Image.readfile(path) to read an ELF file into memory and begin
parsing it.

"""

import collections
import functools
import os
import struct

import glibcpp

class _MetaNamedValue(type):
    """Used to set up _NamedValue subclasses."""

    @classmethod
    def __prepare__(metacls, cls, bases, **kwds):
        # Indicates an int-based class.  Needed for types like Shn.
        int_based = False
        for base in bases:
            if issubclass(base, int):
                int_based = int
                break
        return dict(by_value={},
                    by_name={},
                    prefix=None,
                    _int_based=int_based)

    def __contains__(self, other):
        return other in self.by_value

class _NamedValue(metaclass=_MetaNamedValue):
    """Typed, named integer constants.

    Constants have the following instance attributes:

    name: The full name of the constant (e.g., "PT_NULL").
    short_name: The name with of the constant without the prefix ("NULL").
    value: The integer value of the constant.

    The following class attributes are available:

    by_value: A dict mapping integers to constants.
    by_name: A dict mapping strings to constants.
    prefix: A string that is removed from the start of short names, or None.

    """

    def __new__(cls, arg0, arg1=None):
        """Instance creation.

        For the one-argument form, the argument must be a string, an
        int, or an instance of this class.  Strings are looked up via
        by_name.  Values are looked up via by_value; if value lookup
        fails, a new unnamed instance is returned.  Instances of this
        class a re returned as-is.

        The two-argument form expects the name (a string) and the
        value (an integer).  A new instance is created in this case.
        The instance is not registered in the by_value/by_name
        dictionaries (but the caller can do that).

        """

        typ0 = type(arg0)
        if arg1 is None:
            if isinstance(typ0, cls):
                # Re-use the existing object.
                return arg0
            if typ0 is int:
                by_value = cls.by_value
                try:
                    return by_value[arg0]
                except KeyError:
                    # Create a new object of the requested value.
                    if cls._int_based:
                        result = int.__new__(cls, arg0)
                    else:
                        result = object.__new__(cls)
                    result.value = arg0
                    result.name = None
                    return result
            if typ0 is str:
                by_name = cls.by_name
                try:
                    return by_name[arg0]
                except KeyError:
                    raise ValueError('unknown {} constant: {!r}'.format(
                        cls.__name__, arg0))
        else:
            # Types for the two-argument form are rigid.
            if typ0 is not str and typ0 is not None:
                raise ValueError('type {} of name {!r} should be str'.format(
                    typ0.__name__, arg0))
            if type(arg1) is not int:
                raise ValueError('type {} of value {!r} should be int'.format(
                    type(arg1).__name__, arg1))
            # Create a new named constants.
            if cls._int_based:
                result = int.__new__(cls, arg1)
            else:
                result = object.__new__(cls)
            result.value = arg1
            result.name = arg0
            # Set up the short_name attribute.
            prefix = cls.prefix
            if prefix and arg0.startswith(prefix):
                result.short_name = arg0[len(prefix):]
            else:
                result.short_name = arg0
            return result

    def __str__(self):
        name = self.name
        if name:
            return name
        else:
            return str(self.value)

    def __repr__(self):
        name = self.name
        if name:
            return name
        else:
            return '{}({})'.format(self.__class__.__name__, self.value)

    def __setattr__(self, name, value):
        # Prevent modification of the critical attributes once they
        # have been set.
        if name in ('name', 'value', 'short_name') and hasattr(self, name):
            raise AttributeError('can\'t set attribute {}'.format(name))
        object.__setattr__(self, name, value)

@functools.total_ordering
class _TypedConstant(_NamedValue):
    """Base class for integer-valued optionally named constants.

    This type is not an integer type.

    """

    def __eq__(self, other):
        return isinstance(other, self.__class__) and self.value == other.value

    def __lt__(self, other):
        return isinstance(other, self.__class__) and self.value <= other.value

    def __hash__(self):
        return hash(self.value)

class _IntConstant(_NamedValue, int):
    """Base class for integer-like optionally named constants.

    Instances compare equal to the integer of the same value, and can
    be used in integer arithmetic.

    """

    pass

class _FlagConstant(_TypedConstant, int):
    pass

def _parse_elf_h():
    """Read ../elf/elf.h and return a dict with the constants in it."""

    path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                        '..', 'elf', 'elf.h')
    class TokenizerReporter:
        """Report tokenizer errors to standard output."""

        def __init__(self):
            self.errors = 0

        def error(self, token, message):
            self.errors += 1
            print('{}:{}:{}: error: {}'.format(
                path, token.line, token.column, message))

    reporter = TokenizerReporter()
    with open(path) as inp:
        tokens = glibcpp.tokenize_c(inp.read(), reporter)
    if reporter.errors:
        raise IOError('parse error in elf.h')

    class MacroReporter:
        """Report macro errors to standard output."""

        def __init__(self):
            self.errors = 0

        def error(self, line, message):
            self.errors += 1
            print('{}:{}: error: {}'.format(path, line, message))

        def note(self, line, message):
            print('{}:{}: note: {}'.format(path, line, message))

    reporter = MacroReporter()
    result = glibcpp.macro_eval(glibcpp.macro_definitions(tokens), reporter)
    if reporter.errors:
        raise IOError('parse error in elf.h')

    return result
_elf_h = _parse_elf_h()
del _parse_elf_h
_elf_h_processed = set()

def _register_elf_h(cls, prefix=None, skip=(), ranges=False, parent=None):
    prefix = prefix or cls.prefix
    if not prefix:
        raise ValueError('missing prefix for {}'.format(cls.__name__))
    by_value = cls.by_value
    by_name = cls.by_name
    processed = _elf_h_processed

    skip = set(skip)
    skip.add(prefix + 'NUM')
    if ranges:
        skip.add(prefix + 'LOOS')
        skip.add(prefix + 'HIOS')
        skip.add(prefix + 'LOPROC')
        skip.add(prefix + 'HIPROC')
        cls.os_range = (_elf_h[prefix + 'LOOS'], _elf_h[prefix + 'HIOS'])
        cls.proc_range = (_elf_h[prefix + 'LOPROC'], _elf_h[prefix + 'HIPROC'])

    # Inherit the prefix from the parent if not set.
    if parent and cls.prefix is None and parent.prefix is not None:
        cls.prefix = parent.prefix

    processed_len_start = len(processed)
    for name, value in _elf_h.items():
        if name in skip or name in processed:
            continue
        if name.startswith(prefix):
            processed.add(name)
            if value in by_value:
                raise ValueError('duplicate value {}: {}, {}'.format(
                    value, name, by_value[value]))
            obj = cls(name, value)
            by_value[value] = obj
            by_name[name] = obj
            setattr(cls, name, obj)
            if parent:
                # Make the symbolic name available through the parent as well.
                parent.by_name[name] = obj
                setattr(parent, name, obj)

    if len(processed) == processed_len_start:
        raise ValueError('nothing matched prefix {!r}'.format(prefix))

class ElfClass(_TypedConstant):
    """ELF word size.  Type of EI_CLASS values."""
_register_elf_h(ElfClass, prefix='ELFCLASS')

class ElfData(_TypedConstant):
    """ELF endianness.  Type of EI_DATA values."""
_register_elf_h(ElfData, prefix='ELFDATA')

class Machine(_TypedConstant):
    """ELF machine type.  Type of values in Ehdr.e_machine field."""
    prefix = 'EM_'
_register_elf_h(Machine, skip=('EM_ARC_A5',))

class Et(_TypedConstant):
    """ELF file type.  Type of ET_* values and the Ehdr.e_type field."""
    prefix = 'ET_'
_register_elf_h(Et, ranges=True)

class Shn(_IntConstant):
    """ELF reserved section indices."""
    prefix = 'SHN_'
class ShnMIPS(Shn):
    """Supplemental SHN_* constants for EM_MIPS."""
class ShnPARISC(Shn):
    """Supplemental SHN_* constants for EM_PARISC."""
_register_elf_h(ShnMIPS, prefix='SHN_MIPS_', parent=Shn)
_register_elf_h(ShnPARISC, prefix='SHN_PARISC_', parent=Shn)
_register_elf_h(Shn, skip='SHN_LORESERVE SHN_HIRESERVE'.split(), ranges=True)

class Sht(_TypedConstant):
    """ELF section types.  Type of SHT_* values."""
    prefix = 'SHT_'
class ShtALPHA(Sht):
    """Supplemental SHT_* constants for EM_ALPHA."""
class ShtARC(Sht):
    """Supplemental SHT_* constants for EM_ARC."""
class ShtARM(Sht):
    """Supplemental SHT_* constants for EM_ARM."""
class ShtCSKY(Sht):
    """Supplemental SHT_* constants for EM_CSKY."""
class ShtIA_64(Sht):
    """Supplemental SHT_* constants for EM_IA_64."""
class ShtMIPS(Sht):
    """Supplemental SHT_* constants for EM_MIPS."""
class ShtPARISC(Sht):
    """Supplemental SHT_* constants for EM_PARISC."""
class ShtRISCV(Sht):
    """Supplemental SHT_* constants for EM_RISCV."""
_register_elf_h(ShtALPHA, prefix='SHT_ALPHA_', parent=Sht)
_register_elf_h(ShtARC, prefix='SHT_ARC_', parent=Sht)
_register_elf_h(ShtARM, prefix='SHT_ARM_', parent=Sht)
_register_elf_h(ShtCSKY, prefix='SHT_CSKY_', parent=Sht)
_register_elf_h(ShtIA_64, prefix='SHT_IA_64_', parent=Sht)
_register_elf_h(ShtMIPS, prefix='SHT_MIPS_', parent=Sht)
_register_elf_h(ShtPARISC, prefix='SHT_PARISC_', parent=Sht)
_register_elf_h(ShtRISCV, prefix='SHT_RISCV_', parent=Sht)
_register_elf_h(Sht, ranges=True,
                skip='SHT_LOSUNW SHT_HISUNW SHT_LOUSER SHT_HIUSER'.split())

class Pf(_FlagConstant):
    """Program header flags.  Type of Phdr.p_flags values."""
    prefix = 'PF_'
class PfARM(Pf):
    """Supplemental PF_* flags for EM_ARM."""
class PfHP(Pf):
    """Supplemental PF_* flags for HP-UX."""
class PfIA_64(Pf):
    """Supplemental PF_* flags for EM_IA_64."""
class PfMIPS(Pf):
    """Supplemental PF_* flags for EM_MIPS."""
class PfPARISC(Pf):
    """Supplemental PF_* flags for EM_PARISC."""
_register_elf_h(PfARM, prefix='PF_ARM_', parent=Pf)
_register_elf_h(PfHP, prefix='PF_HP_', parent=Pf)
_register_elf_h(PfIA_64, prefix='PF_IA_64_', parent=Pf)
_register_elf_h(PfMIPS, prefix='PF_MIPS_', parent=Pf)
_register_elf_h(PfPARISC, prefix='PF_PARISC_', parent=Pf)
_register_elf_h(Pf, skip='PF_MASKOS PF_MASKPROC'.split())

class Shf(_FlagConstant):
    """Section flags.  Type of Shdr.sh_type values."""
    prefix = 'SHF_'
class ShfALPHA(Shf):
    """Supplemental SHF_* constants for EM_ALPHA."""
class ShfARM(Shf):
    """Supplemental SHF_* constants for EM_ARM."""
class ShfIA_64(Shf):
    """Supplemental SHF_* constants for EM_IA_64."""
class ShfMIPS(Shf):
    """Supplemental SHF_* constants for EM_MIPS."""
class ShfPARISC(Shf):
    """Supplemental SHF_* constants for EM_PARISC."""
_register_elf_h(ShfALPHA, prefix='SHF_ALPHA_', parent=Shf)
_register_elf_h(ShfARM, prefix='SHF_ARM_', parent=Shf)
_register_elf_h(ShfIA_64, prefix='SHF_IA_64_', parent=Shf)
_register_elf_h(ShfMIPS, prefix='SHF_MIPS_', parent=Shf)
_register_elf_h(ShfPARISC, prefix='SHF_PARISC_', parent=Shf)
_register_elf_h(Shf, skip='SHF_MASKOS SHF_MASKPROC'.split())

class Stb(_TypedConstant):
    """ELF symbol binding type."""
    prefix = 'STB_'
_register_elf_h(Stb, ranges=True)

class Stt(_TypedConstant):
    """ELF symbol type."""
    prefix = 'STT_'
class SttARM(Sht):
    """Supplemental STT_* constants for EM_ARM."""
class SttPARISC(Sht):
    """Supplemental STT_* constants for EM_PARISC."""
class SttSPARC(Sht):
    """Supplemental STT_* constants for EM_SPARC."""
    STT_SPARC_REGISTER = 13
class SttX86_64(Sht):
    """Supplemental STT_* constants for EM_X86_64."""
_register_elf_h(SttARM, prefix='STT_ARM_', parent=Stt)
_register_elf_h(SttPARISC, prefix='STT_PARISC_', parent=Stt)
_register_elf_h(SttSPARC, prefix='STT_SPARC_', parent=Stt)
_register_elf_h(Stt, ranges=True)


class Pt(_TypedConstant):
    """ELF program header types.  Type of Phdr.p_type."""
    prefix = 'PT_'
class PtAARCH64(Pt):
    """Supplemental PT_* constants for EM_AARCH64."""
class PtARM(Pt):
    """Supplemental PT_* constants for EM_ARM."""
class PtHP(Pt):
    """Supplemental PT_* constants for HP-U."""
class PtIA_64(Pt):
    """Supplemental PT_* constants for EM_IA_64."""
class PtMIPS(Pt):
    """Supplemental PT_* constants for EM_MIPS."""
class PtPARISC(Pt):
    """Supplemental PT_* constants for EM_PARISC."""
class PtRISCV(Pt):
    """Supplemental PT_* constants for EM_RISCV."""
_register_elf_h(PtAARCH64, prefix='PT_AARCH64_', parent=Pt)
_register_elf_h(PtARM, prefix='PT_ARM_', parent=Pt)
_register_elf_h(PtHP, prefix='PT_HP_', parent=Pt)
_register_elf_h(PtIA_64, prefix='PT_IA_64_', parent=Pt)
_register_elf_h(PtMIPS, prefix='PT_MIPS_', parent=Pt)
_register_elf_h(PtPARISC, prefix='PT_PARISC_', parent=Pt)
_register_elf_h(PtRISCV, prefix='PT_RISCV_', parent=Pt)
_register_elf_h(Pt, skip='PT_LOSUNW PT_HISUNW'.split(), ranges=True)

class Dt(_TypedConstant):
    """ELF dynamic segment tags.  Type of Dyn.d_val."""
    prefix = 'DT_'
class DtAARCH64(Dt):
    """Supplemental DT_* constants for EM_AARCH64."""
class DtALPHA(Dt):
    """Supplemental DT_* constants for EM_ALPHA."""
class DtALTERA_NIOS2(Dt):
    """Supplemental DT_* constants for EM_ALTERA_NIOS2."""
class DtIA_64(Dt):
    """Supplemental DT_* constants for EM_IA_64."""
class DtMIPS(Dt):
    """Supplemental DT_* constants for EM_MIPS."""
class DtPPC(Dt):
    """Supplemental DT_* constants for EM_PPC."""
class DtPPC64(Dt):
    """Supplemental DT_* constants for EM_PPC64."""
class DtRISCV(Dt):
    """Supplemental DT_* constants for EM_RISCV."""
class DtSPARC(Dt):
    """Supplemental DT_* constants for EM_SPARC."""
class DtX86_64(Dt):
    """Supplemental DT_* constants for EM_X86_64."""
_dt_skip = '''
DT_ENCODING DT_PROCNUM
DT_ADDRRNGLO DT_ADDRRNGHI DT_ADDRNUM
DT_VALRNGLO DT_VALRNGHI DT_VALNUM
DT_VERSIONTAGNUM DT_EXTRANUM
DT_AARCH64_NUM
DT_ALPHA_NUM
DT_IA_64_NUM
DT_MIPS_NUM
DT_PPC_NUM
DT_PPC64_NUM
DT_SPARC_NUM
DT_X86_64_NUM
'''.strip().split()
_register_elf_h(DtAARCH64, prefix='DT_AARCH64_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtALPHA, prefix='DT_ALPHA_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtALTERA_NIOS2, prefix='DT_NIOS2_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtIA_64, prefix='DT_IA_64_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtMIPS, prefix='DT_MIPS_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtPPC, prefix='DT_PPC_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtPPC64, prefix='DT_PPC64_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtRISCV, prefix='DT_RISCV_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtSPARC, prefix='DT_SPARC_', skip=_dt_skip, parent=Dt)
_register_elf_h(DtX86_64, prefix='DT_X86_64_', skip=_dt_skip, parent=Dt)
_register_elf_h(Dt, skip=_dt_skip, ranges=True)
del _dt_skip

# Constant extraction is complete.
del _register_elf_h
del _elf_h

class StInfo:
    """ELF symbol binding and type.  Type of the Sym.st_info field."""
    def __init__(self, arg0, arg1=None):
        if isinstance(arg0, int) and arg1 is None:
            self.bind = Stb(arg0 >> 4)
            self.type = Stt(arg0 & 15)
        else:
            self.bind = Stb(arg0)
            self.type = Stt(arg1)

    def value(self):
        """Returns the raw value for the bind/type combination."""
        return (self.bind.value() << 4) | (self.type.value())

# Type in an ELF file.  Used for deserialization.
_Layout = collections.namedtuple('_Layout', 'unpack size')

def _define_layouts(baseclass: type, layout32: str, layout64: str,
                    types=None, fields32=None):
    """Assign variants dict to baseclass.

    The variants dict is indexed by (ElfClass, ElfData) pairs, and its
    values are _Layout instances.

    """
    struct32 = struct.Struct(layout32)
    struct64 = struct.Struct(layout64)

    # Check that the struct formats yield the right number of components.
    for s in (struct32, struct64):
        example = s.unpack(b' ' * s.size)
        if len(example) != len(baseclass._fields):
            raise ValueError('{!r} yields wrong field count: {} != {}'.format(
                s.format, len(example),  len(baseclass._fields)))

    # Check that field names in types are correct.
    if types is None:
        types = ()
    for n in types:
        if n not in baseclass._fields:
            raise ValueError('{} does not have field {!r}'.format(
                baseclass.__name__, n))

    if fields32 is not None \
       and set(fields32) != set(baseclass._fields):
        raise ValueError('{!r} is not a permutation of the fields {!r}'.format(
            fields32, baseclass._fields))

    def unique_name(name, used_names = (set((baseclass.__name__,))
                                        | set(baseclass._fields)
                                        | {n.__name__
                                           for n in (types or {}).values()})):
        """Find a name that is not used for a class or field name."""
        candidate = name
        n = 0
        while candidate in used_names:
            n += 1
            candidate = '{}{}'.format(name, n)
        used_names.add(candidate)
        return candidate

    blob_name = unique_name('blob')
    struct_unpack_name = unique_name('struct_unpack')
    comps_name = unique_name('comps')

    layouts = {}
    for (bits, elfclass, layout, fields) in (
            (32, ElfClass.ELFCLASS32, layout32, fields32),
            (64, ElfClass.ELFCLASS64, layout64, None),
    ):
        for (elfdata, structprefix, funcsuffix) in (
                (ElfData.ELFDATA2LSB, '<', 'LE'),
                (ElfData.ELFDATA2MSB, '>', 'BE'),
        ):
            env = {
                baseclass.__name__: baseclass,
                struct_unpack_name: struct.unpack,
            }

            # Add the type converters.
            if types:
                for cls in types.values():
                    env[cls.__name__] = cls

            funcname = ''.join(
                ('unpack_', baseclass.__name__, str(bits), funcsuffix))

            code = '''
def {funcname}({blob_name}):
'''.format(funcname=funcname, blob_name=blob_name)

            indent = ' ' * 4
            unpack_call = '{}({!r}, {})'.format(
                struct_unpack_name, structprefix + layout, blob_name)
            field_names = ', '.join(baseclass._fields)
            if types is None and fields is None:
                code += '{}return {}({})\n'.format(
                    indent, baseclass.__name__, unpack_call)
            else:
                # Destructuring tuple assignment.
                if fields is None:
                    code += '{}{} = {}\n'.format(
                        indent, field_names, unpack_call)
                else:
                    # Use custom field order.
                    code += '{}{} = {}\n'.format(
                        indent, ', '.join(fields), unpack_call)

                # Perform the type conversions.
                for n in baseclass._fields:
                    if n in types:
                        code += '{}{} = {}({})\n'.format(
                            indent, n, types[n].__name__, n)
                # Create the named tuple.
                code += '{}return {}({})\n'.format(
                    indent, baseclass.__name__, field_names)

            exec(code, env)
            layouts[(elfclass, elfdata)] = _Layout(
                env[funcname], struct.calcsize(layout))
    baseclass.layouts = layouts


# Corresponds to EI_* indices into Elf*_Ehdr.e_indent.
class Ident(collections.namedtuple('Ident',
    'ei_mag ei_class ei_data ei_version ei_osabi ei_abiversion ei_pad')):

    def __new__(cls, *args):
        """Construct an object from a blob or its constituent fields."""
        if len(args) == 1:
            return cls.unpack(args[0])
        return cls.__base__.__new__(cls, *args)

    @staticmethod
    def unpack(blob: memoryview) -> 'Ident':
        """Parse raws data into a tuple."""
        ei_mag, ei_class, ei_data, ei_version, ei_osabi, ei_abiversion, \
            ei_pad = struct.unpack('4s5B7s', blob)
        return Ident(ei_mag, ElfClass(ei_class), ElfData(ei_data),
                     ei_version, ei_osabi, ei_abiversion, ei_pad)
    size = 16

# Corresponds to Elf32_Ehdr and Elf64_Ehdr.
Ehdr = collections.namedtuple('Ehdr',
   'e_ident e_type e_machine e_version e_entry e_phoff e_shoff e_flags'
    + ' e_ehsize e_phentsize e_phnum e_shentsize e_shnum e_shstrndx')
_define_layouts(Ehdr,
                layout32='16s2H5I6H',
                layout64='16s2HI3QI6H',
                types=dict(e_ident=Ident,
                           e_machine=Machine,
                           e_type=Et,
                           e_shstrndx=Shn))

# Corresponds to Elf32_Phdr and Elf64_Pdhr.  Order follows the latter.
Phdr = collections.namedtuple('Phdr',
    'p_type p_flags p_offset p_vaddr p_paddr p_filesz p_memsz p_align')
_define_layouts(Phdr,
                layout32='8I',
                fields32=('p_type', 'p_offset', 'p_vaddr', 'p_paddr',
                          'p_filesz', 'p_memsz', 'p_flags', 'p_align'),
                layout64='2I6Q',
            types=dict(p_type=Pt, p_flags=Pf))


# Corresponds to Elf32_Shdr and Elf64_Shdr.
class Shdr(collections.namedtuple('Shdr',
    'sh_name sh_type sh_flags sh_addr sh_offset sh_size sh_link sh_info'
    + ' sh_addralign sh_entsize')):
    def resolve(self, strtab: 'StringTable') -> 'Shdr':
        """Resolve sh_name using a string table."""
        return self.__class__(strtab.get(self[0]), *self[1:])
_define_layouts(Shdr,
                layout32='10I',
                layout64='2I4Q2I2Q',
                types=dict(sh_type=Sht,
                           sh_flags=Shf,
                           sh_link=Shn))

# Corresponds to Elf32_Dyn and Elf64_Dyn.  The nesting through the
# d_un union is skipped, and d_ptr is missing (its representation in
# Python would be identical to d_val).
Dyn = collections.namedtuple('Dyn', 'd_tag d_val')
_define_layouts(Dyn,
                layout32='2i',
                layout64='2q',
                types=dict(d_tag=Dt))

# Corresponds to Elf32_Sym and Elf64_Sym.
class Sym(collections.namedtuple('Sym',
    'st_name st_info st_other st_shndx st_value st_size')):
    def resolve(self, strtab: 'StringTable') -> 'Sym':
        """Resolve st_name using a string table."""
        return self.__class__(strtab.get(self[0]), *self[1:])
_define_layouts(Sym,
                layout32='3I2BH',
                layout64='I2BH2Q',
                fields32=('st_name', 'st_value', 'st_size', 'st_info',
                          'st_other', 'st_shndx'),
                types=dict(st_shndx=Shn,
                           st_info=StInfo))

# Corresponds to Elf32_Rel and Elf64_Rel.
Rel = collections.namedtuple('Rel', 'r_offset r_info')
_define_layouts(Rel,
                layout32='2I',
                layout64='2Q')

# Corresponds to Elf32_Rel and Elf64_Rel.
Rela = collections.namedtuple('Rela', 'r_offset r_info r_addend')
_define_layouts(Rela,
                layout32='3I',
                layout64='3Q')

class StringTable:
    """ELF string table."""
    def __init__(self, blob):
        """Create a new string table backed by the data in the blob.

        blob: a memoryview-like object

        """
        self.blob = blob

    def get(self, index) -> bytes:
        """Returns the null-terminated byte string at the index."""
        blob = self.blob
        endindex = index
        while True:
            if blob[endindex] == 0:
                return bytes(blob[index:endindex])
            endindex += 1

class Image:
    """ELF image parser."""
    def __init__(self, image):
        """Create an ELF image from binary image data.

        image: a memoryview-like object that supports efficient range
        subscripting.

        """
        self.image = image
        ident = self.read(Ident, 0)
        classdata = (ident.ei_class, ident.ei_data)
        # Set self.Ehdr etc. to the subtypes with the right parsers.
        for typ in (Ehdr, Phdr, Shdr, Dyn, Sym, Rel, Rela):
            setattr(self, typ.__name__, typ.layouts.get(classdata, None))

        if self.Ehdr is not None:
            self.ehdr = self.read(self.Ehdr, 0)
            self._shdr_num = self._compute_shdr_num()
        else:
            self.ehdr = None
            self._shdr_num = 0

        self._section = {}
        self._stringtab = {}

        if self._shdr_num > 0:
            self._shdr_strtab = self._find_shdr_strtab()
        else:
            self._shdr_strtab = None

    @staticmethod
    def readfile(path: str) -> 'Image':
        """Reads the ELF file at the specified path."""
        with open(path, 'rb') as inp:
            return Image(memoryview(inp.read()))

    def _compute_shdr_num(self) -> int:
        """Computes the actual number of section headers."""
        shnum = self.ehdr.e_shnum
        if shnum == 0:
            if self.ehdr.e_shoff == 0 or self.ehdr.e_shentsize == 0:
                # No section headers.
                return 0
            # Otherwise the extension mechanism is used (which may be
            # needed because e_shnum is just 16 bits).
            return self.read(self.Shdr, self.ehdr.e_shoff).sh_size
        return shnum

    def _find_shdr_strtab(self) -> StringTable:
        """Finds the section header string table (maybe via extensions)."""
        shstrndx = self.ehdr.e_shstrndx
        if shstrndx == Shn.SHN_XINDEX:
            shstrndx = self.read(self.Shdr, self.ehdr.e_shoff).sh_link
        return self._find_stringtab(shstrndx)

    def read(self, typ: type, offset:int ):
        """Reads an object at a specific offset.

        The type must have been enhanced using _define_variants.

        """
        return typ.unpack(self.image[offset: offset + typ.size])

    def phdrs(self) -> Phdr:
        """Generator iterating over the program headers."""
        if self.ehdr is None:
            return
        size = self.ehdr.e_phentsize
        if size != self.Phdr.size:
            raise ValueError('Unexpected Phdr size in ELF header: {} != {}'
                             .format(size, self.Phdr.size))

        offset = self.ehdr.e_phoff
        for _ in range(self.ehdr.e_phnum):
            yield self.read(self.Phdr, offset)
            offset += size

    def shdrs(self, resolve: bool=True) -> Shdr:
        """Generator iterating over the section headers.

        If resolve, section names are automatically translated
        using the section header string table.

        """
        if self._shdr_num == 0:
            return

        size = self.ehdr.e_shentsize
        if size != self.Shdr.size:
            raise ValueError('Unexpected Shdr size in ELF header: {} != {}'
                             .format(size, self.Shdr.size))

        offset = self.ehdr.e_shoff
        for _ in range(self._shdr_num):
            shdr = self.read(self.Shdr, offset)
            if resolve:
                shdr = shdr.resolve(self._shdr_strtab)
            yield shdr
            offset += size

    def dynamic(self) -> Dyn:
        """Generator iterating over the dynamic segment."""
        for phdr in self.phdrs():
            if phdr.p_type == Pt.PT_DYNAMIC:
                # Pick the first dynamic segment, like the loader.
                if phdr.p_filesz == 0:
                    # Probably separated debuginfo.
                    return
                offset = phdr.p_offset
                end = offset + phdr.p_memsz
                size = self.Dyn.size
                while True:
                    next_offset = offset + size
                    if next_offset > end:
                        raise ValueError(
                            'Dynamic segment size {} is not a multiple of Dyn size {}'.format(
                                phdr.p_memsz, size))
                    yield self.read(self.Dyn, offset)
                    if next_offset == end:
                        return
                    offset = next_offset

    def syms(self, shdr: Shdr, resolve: bool=True) -> Sym:
        """A generator iterating over a symbol table.

        If resolve, symbol names are automatically translated using
        the string table for the symbol table.

        """
        assert shdr.sh_type == Sht.SHT_SYMTAB
        size = shdr.sh_entsize
        if size != self.Sym.size:
            raise ValueError('Invalid symbol table entry size {}'.format(size))
        offset = shdr.sh_offset
        end = shdr.sh_offset + shdr.sh_size
        if resolve:
            strtab = self._find_stringtab(shdr.sh_link)
        while offset < end:
            sym = self.read(self.Sym, offset)
            if resolve:
                sym = sym.resolve(strtab)
            yield sym
            offset += size
        if offset != end:
            raise ValueError('Symbol table is not a multiple of entry size')

    def lookup_string(self, strtab_index: int, strtab_offset: int) -> bytes:
        """Looks up a string in a string table identified by its link index."""
        try:
            strtab = self._stringtab[strtab_index]
        except KeyError:
            strtab = self._find_stringtab(strtab_index)
        return strtab.get(strtab_offset)

    def find_section(self, shndx: Shn) -> Shdr:
        """Returns the section header for the indexed section.

        The section name is not resolved.
        """
        try:
            return self._section[shndx]
        except KeyError:
            pass
        if shndx in Shn:
            raise ValueError('Reserved section index {}'.format(shndx))
        idx = shndx.value
        if idx < 0 or idx > self._shdr_num:
            raise ValueError('Section index {} out of range [0, {})'.format(
                idx, self._shdr_num))
        shdr = self.read(
            self.Shdr, self.ehdr.e_shoff + idx * self.Shdr.size)
        self._section[shndx] = shdr
        return shdr

    def _find_stringtab(self, sh_link: int) -> StringTable:
        if sh_link in self._stringtab:
            return self._stringtab
        if sh_link < 0 or sh_link >= self._shdr_num:
            raise ValueError('Section index {} out of range [0, {})'.format(
                sh_link, self._shdr_num))
        shdr = self.read(
            self.Shdr, self.ehdr.e_shoff + sh_link * self.Shdr.size)
        if shdr.sh_type != Sht.SHT_STRTAB:
            raise ValueError(
                'Section {} is not a string table: {}'.format(
                    sh_link, shdr.sh_type))
        strtab = StringTable(
            self.image[shdr.sh_offset:shdr.sh_offset + shdr.sh_size])
        # This could retrain essentially arbitrary amounts of data,
        # but caching string tables seems important for performance.
        self._stringtab[sh_link] = strtab
        return strtab

def elf_hash(s):
    """Computes the ELF hash of the string."""
    acc = 0
    for ch in s:
        if type(ch) is not int:
            ch = ord(ch)
        acc = ((acc << 4) + ch) & 0xffffffff
        top = acc & 0xf0000000
        acc = (acc ^ (top >> 24)) & ~top
    return acc

def gnu_hash(s):
    """Computes the GNU hash of the string."""
    h = 5381
    for ch in s:
        if type(ch) is not int:
            ch = ord(ch)
        h = (h * 33 + ch) & 0xffffffff
    return h

__all__ = [name for name in dir() if name[0].isupper()]
