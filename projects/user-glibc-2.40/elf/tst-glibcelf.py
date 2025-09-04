#!/usr/bin/python3
# Verify scripts/glibcelf.py contents against elf/elf.h.
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

import argparse
import sys

import glibcelf
import glibcextract

errors_encountered = 0

def error(message):
    global errors_encountered
    sys.stdout.write('error: {}\n'.format(message))
    errors_encountered += 1

# The enum constants in glibcelf are expected to have exactly these
# prefixes.
expected_constant_prefixes = tuple(
    'ELFCLASS ELFDATA EM_ ET_ DT_ PF_ PT_ SHF_ SHN_ SHT_ STB_ STT_'.split())

def find_constant_prefix(name):
    """Returns a matching prefix from expected_constant_prefixes or None."""
    for prefix in expected_constant_prefixes:
        if name.startswith(prefix):
            return prefix
    return None

def find_enum_types():
    """A generator for OpenIntEnum and IntFlag classes in glibcelf."""
    classes = set((glibcelf._TypedConstant, glibcelf._IntConstant,
                   glibcelf._FlagConstant))
    for obj in vars(glibcelf).values():
        if isinstance(obj, type) and obj not in classes \
           and obj.__bases__[0] in classes:
            yield obj

def check_basic():
    """Check basic functionality of the constant classes."""

    if glibcelf.Pt.PT_NULL is not glibcelf.Pt(0):
        error('Pt(0) not interned')
    if glibcelf.Pt(17609) is glibcelf.Pt(17609):
        error('Pt(17609) unexpectedly interned')
    if glibcelf.Pt(17609) == glibcelf.Pt(17609):
        pass
    else:
        error('Pt(17609) equality')
    if glibcelf.Pt(17610) == glibcelf.Pt(17609):
        error('Pt(17610) equality')

    if str(glibcelf.Pt.PT_NULL) != 'PT_NULL':
        error('str(PT_NULL)')
    if str(glibcelf.Pt(17609)) != '17609':
        error('str(Pt(17609))')

    if repr(glibcelf.Pt.PT_NULL) != 'PT_NULL':
        error('repr(PT_NULL)')
    if repr(glibcelf.Pt(17609)) != 'Pt(17609)':
        error('repr(Pt(17609))')

    if glibcelf.Pt('PT_AARCH64_MEMTAG_MTE') \
       is not glibcelf.Pt.PT_AARCH64_MEMTAG_MTE:
        error('PT_AARCH64_MEMTAG_MTE identity')
    if glibcelf.Pt(0x70000002) is glibcelf.Pt.PT_AARCH64_MEMTAG_MTE:
        error('Pt(0x70000002) identity')
    if glibcelf.PtAARCH64(0x70000002) is not glibcelf.Pt.PT_AARCH64_MEMTAG_MTE:
        error('PtAARCH64(0x70000002) identity')
    if glibcelf.Pt.PT_AARCH64_MEMTAG_MTE.short_name != 'AARCH64_MEMTAG_MTE':
        error('PT_AARCH64_MEMTAG_MTE short name')

    # Special cases for int-like Shn.
    if glibcelf.Shn(32) == glibcelf.Shn.SHN_XINDEX:
        error('Shn(32)')
    if glibcelf.Shn(32) + 0 != 32:
        error('Shn(32) + 0')
    if 32 in glibcelf.Shn:
        error('32 in Shn')
    if 0 not in glibcelf.Shn:
        error('0 not in Shn')

def check_duplicates():
    """Verifies that enum types do not have duplicate values.

    Different types must have different member names, too.

    """
    global_seen = {}
    for typ in find_enum_types():
        seen = {}
        for (name, e) in typ.by_name.items():
            if e.value in seen:
                other = seen[e.value]
                # Value conflicts only count if they are between
                # the same base type.
                if e.__class__ is typ and other.__class__ is typ:
                    error('{} has {}={} and {}={}'.format(
                        typ, other, e.value, name, e.value))
            else:
                seen[e.value] = name
                if name in global_seen:
                    error('{} used in {} and {}'.format(
                        name, global_seen[name], typ))
                else:
                    global_seen[name] = typ

def check_constant_prefixes():
    """Check that the constant prefixes match expected_constant_prefixes."""
    seen = set()
    for typ in find_enum_types():
        typ_prefix = None
        for val in typ.by_name.values():
            prefix = find_constant_prefix(val.name)
            if prefix is None:
                error('constant {!r} for {} has unknown prefix'.format(
                    val, typ))
                break
            elif typ_prefix is None:
                typ_prefix = prefix
                seen.add(typ_prefix)
            elif prefix != typ_prefix:
                error('prefix {!r} for constant {!r}, expected {!r}'.format(
                    prefix, val, typ_prefix))
        if typ_prefix is None:
            error('empty enum type {}'.format(typ))

    for prefix in sorted(set(expected_constant_prefixes) - seen):
        error('missing constant prefix {!r}'.format(prefix))
    # Reverse difference is already covered inside the loop.

def find_elf_h_constants(cc):
    """Returns a dictionary of relevant constants from <elf.h>."""
    return glibcextract.compute_macro_consts(
        source_text='#include <elf.h>',
        cc=cc,
        macro_re='|'.join(
            prefix + '.*' for prefix in expected_constant_prefixes))

# The first part of the pair is a name of an <elf.h> constant that is
# dropped from glibcelf.  The second part is the constant as it is
# used in <elf.h>.
glibcelf_skipped_aliases = (
    ('EM_ARC_A5', 'EM_ARC_COMPACT'),
)

# Constants that provide little value and are not included in
# glibcelf: *LO*/*HI* range constants, *NUM constants counting the
# number of constants.  Also includes the alias names from
# glibcelf_skipped_aliases.
glibcelf_skipped_constants = frozenset(
    [e[0] for e in glibcelf_skipped_aliases]) | frozenset("""
DT_AARCH64_NUM
DT_ADDRNUM
DT_ADDRRNGHI
DT_ADDRRNGLO
DT_ALPHA_NUM
DT_ENCODING
DT_EXTRANUM
DT_HIOS
DT_HIPROC
DT_IA_64_NUM
DT_LOOS
DT_LOPROC
DT_MIPS_NUM
DT_NUM
DT_PPC64_NUM
DT_PPC_NUM
DT_PROCNUM
DT_SPARC_NUM
DT_VALNUM
DT_VALRNGHI
DT_VALRNGLO
DT_VERSIONTAGNUM
DT_X86_64_NUM
ELFCLASSNUM
ELFDATANUM
EM_NUM
ET_HIOS
ET_HIPROC
ET_LOOS
ET_LOPROC
ET_NUM
PF_MASKOS
PF_MASKPROC
PT_HIOS
PT_HIPROC
PT_HISUNW
PT_LOOS
PT_LOPROC
PT_LOSUNW
PT_NUM
SHF_MASKOS
SHF_MASKPROC
SHN_HIOS
SHN_HIPROC
SHN_HIRESERVE
SHN_LOOS
SHN_LOPROC
SHN_LORESERVE
SHT_HIOS
SHT_HIPROC
SHT_HIPROC
SHT_HISUNW
SHT_HIUSER
SHT_LOOS
SHT_LOPROC
SHT_LOSUNW
SHT_LOUSER
SHT_NUM
STB_HIOS
STB_HIPROC
STB_LOOS
STB_LOPROC
STB_NUM
STT_HIOS
STT_HIPROC
STT_LOOS
STT_LOPROC
STT_NUM
""".strip().split())

def check_constant_values(cc):
    """Checks the values of <elf.h> constants against glibcelf."""

    glibcelf_constants = {
        e.name: e for typ in find_enum_types() for e in typ.by_name.values()}
    elf_h_constants = find_elf_h_constants(cc=cc)

    missing_in_glibcelf = (set(elf_h_constants) - set(glibcelf_constants)
                           - glibcelf_skipped_constants)
    for name in sorted(missing_in_glibcelf):
        error('constant {} is missing from glibcelf'.format(name))

    unexpected_in_glibcelf = \
        set(glibcelf_constants) & glibcelf_skipped_constants
    for name in sorted(unexpected_in_glibcelf):
        error('constant {} is supposed to be filtered from glibcelf'.format(
            name))

    missing_in_elf_h = set(glibcelf_constants) - set(elf_h_constants)
    for name in sorted(missing_in_elf_h):
        error('constant {} is missing from <elf.h>'.format(name))

    expected_in_elf_h = glibcelf_skipped_constants - set(elf_h_constants)
    for name in expected_in_elf_h:
        error('filtered constant {} is missing from <elf.h>'.format(name))

    for alias_name, name_in_glibcelf in glibcelf_skipped_aliases:
        if name_in_glibcelf not in glibcelf_constants:
            error('alias value {} for {} not in glibcelf'.format(
                name_in_glibcelf, alias_name))
        elif (int(elf_h_constants[alias_name])
              != glibcelf_constants[name_in_glibcelf].value):
            error('<elf.h> has {}={}, glibcelf has {}={}'.format(
                alias_name, elf_h_constants[alias_name],
                name_in_glibcelf, glibcelf_constants[name_in_glibcelf]))

    # Check for value mismatches:
    for name in sorted(set(glibcelf_constants) & set(elf_h_constants)):
        glibcelf_value = glibcelf_constants[name].value
        elf_h_value = int(elf_h_constants[name])
        # On 32-bit architectures <elf.h> has some constants that are
        # parsed as signed, while they are unsigned in glibcelf.  So
        # far, this only affects some flag constants, so special-case
        # them here.
        if (glibcelf_value != elf_h_value
            and not (isinstance(glibcelf_constants[name],
                                glibcelf._FlagConstant)
                     and glibcelf_value == 1 << 31
                     and elf_h_value == -(1 << 31))):
            error('{}: glibcelf has {!r}, <elf.h> has {!r}'.format(
                name, glibcelf_value, elf_h_value))

def check_hashes():
    for name, expected_elf, expected_gnu in (
            ('', 0, 0x1505),
            ('PPPPPPPPPPPP', 0, 0x9f105c45),
            ('GLIBC_2.0', 0xd696910, 0xf66c3dd5),
            ('GLIBC_2.34', 0x69691b4, 0xc3f3f90c),
            ('GLIBC_PRIVATE', 0x963cf85, 0x692a260)):
        for convert in (lambda x: x, lambda x: x.encode('UTF-8')):
            name = convert(name)
            actual_elf = glibcelf.elf_hash(name)
            if actual_elf != expected_elf:
                error('elf_hash({!r}): {:x} != 0x{:x}'.format(
                    name, actual_elf, expected_elf))
            actual_gnu = glibcelf.gnu_hash(name)
            if actual_gnu != expected_gnu:
                error('gnu_hash({!r}): {:x} != 0x{:x}'.format(
                    name, actual_gnu, expected_gnu))

def main():
    """The main entry point."""
    parser = argparse.ArgumentParser(
        description="Check glibcelf.py and elf.h against each other.")
    parser.add_argument('--cc', metavar='CC',
                        help='C compiler (including options) to use')
    args = parser.parse_args()

    check_basic()
    check_duplicates()
    check_constant_prefixes()
    check_constant_values(cc=args.cc)
    check_hashes()

    if errors_encountered > 0:
        print("note: errors encountered:", errors_encountered)
        sys.exit(1)

if __name__ == '__main__':
    main()
