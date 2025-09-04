#!/usr/bin/python3
# ELF editor for load align tests.
# Copyright (C) 2022-2024 Free Software Foundation, Inc.
# Copyright The GNU Toolchain Authors.
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
import os
import sys
import struct

EI_NIDENT=16

EI_MAG0=0
ELFMAG0=b'\x7f'
EI_MAG1=1
ELFMAG1=b'E'
EI_MAG2=2
ELFMAG2=b'L'
EI_MAG3=3
ELFMAG3=b'F'

EI_CLASS=4
ELFCLASSNONE=b'0'
ELFCLASS32=b'\x01'
ELFCLASS64=b'\x02'

EI_DATA=5
ELFDATA2LSB=b'\x01'
ELFDATA2MSB=b'\x02'

ET_EXEC=2
ET_DYN=3

PT_LOAD=1
PT_TLS=7

def elf_types_fmts(e_ident):
    endian = '<' if e_ident[EI_DATA] == ELFDATA2LSB else '>'
    addr = 'I' if e_ident[EI_CLASS] == ELFCLASS32 else 'Q'
    off = 'I' if e_ident[EI_CLASS] == ELFCLASS32 else 'Q'
    return (endian, addr, off)

class Elf_Ehdr:
    def __init__(self, e_ident):
        endian, addr, off = elf_types_fmts(e_ident)
        self.fmt = '{0}HHI{1}{2}{2}IHHHHHH'.format(endian, addr, off)
        self.len = struct.calcsize(self.fmt)

    def read(self, f):
        buf = f.read(self.len)
        if not buf:
            error('{}: header too small'.format(f.name))
        data = struct.unpack(self.fmt, buf)
        self.e_type = data[0]
        self.e_machine = data[1]
        self.e_version = data[2]
        self.e_entry = data[3]
        self.e_phoff = data[4]
        self.e_shoff = data[5]
        self.e_flags = data[6]
        self.e_ehsize = data[7]
        self.e_phentsize= data[8]
        self.e_phnum = data[9]
        self.e_shstrndx = data[10]


class Elf_Phdr:
    def __init__(self, e_ident):
        endian, addr, off = elf_types_fmts(e_ident)
        self.ei_class = e_ident[EI_CLASS]
        if self.ei_class == ELFCLASS32:
            self.fmt = '{0}I{2}{1}{1}IIII'.format(endian, addr, off)
        else:
            self.fmt = '{0}II{2}{1}{1}QQQ'.format(endian, addr, off)
        self.len = struct.calcsize(self.fmt)

    def read(self, f):
        buf = f.read(self.len)
        if len(buf) < self.len:
            error('{}: program header too small'.format(f.name))
        data = struct.unpack(self.fmt, buf)
        if self.ei_class == ELFCLASS32:
            self.p_type = data[0]
            self.p_offset = data[1]
            self.p_vaddr = data[2]
            self.p_paddr = data[3]
            self.p_filesz = data[4]
            self.p_memsz = data[5]
            self.p_flags = data[6]
            self.p_align = data[7]
        else:
            self.p_type = data[0]
            self.p_flags = data[1]
            self.p_offset = data[2]
            self.p_vaddr = data[3]
            self.p_paddr = data[4]
            self.p_filesz = data[5]
            self.p_memsz = data[6]
            self.p_align = data[7]

    def write(self, f):
        if self.ei_class == ELFCLASS32:
            data = struct.pack(self.fmt,
                               self.p_type,
                               self.p_offset,
                               self.p_vaddr,
                               self.p_paddr,
                               self.p_filesz,
                               self.p_memsz,
                               self.p_flags,
                               self.p_align)
        else:
            data = struct.pack(self.fmt,
                               self.p_type,
                               self.p_flags,
                               self.p_offset,
                               self.p_vaddr,
                               self.p_paddr,
                               self.p_filesz,
                               self.p_memsz,
                               self.p_align)
        f.write(data)


def error(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


def elf_edit_align(phdr, align):
    if align == 'half':
        phdr.p_align = phdr.p_align >> 1
    else:
        phdr.p_align = int(align)

def elf_edit_maximize_tls_size(phdr, elfclass):
    if elfclass == ELFCLASS32:
        # It is possible that the kernel can allocate half of the
        # address space, so use something larger.
        phdr.p_memsz = 0xfff00000
    else:
        phdr.p_memsz = 1 << 63

def elf_edit(f, opts):
    ei_nident_fmt = 'c' * EI_NIDENT
    ei_nident_len = struct.calcsize(ei_nident_fmt)

    data = f.read(ei_nident_len)
    if len(data) < ei_nident_len:
      error('{}: e_nident too small'.format(f.name))
    e_ident = struct.unpack(ei_nident_fmt, data)

    if e_ident[EI_MAG0] != ELFMAG0 \
       or e_ident[EI_MAG1] != ELFMAG1 \
       or e_ident[EI_MAG2] != ELFMAG2 \
       or e_ident[EI_MAG3] != ELFMAG3:
      error('{}: bad ELF header'.format(f.name))

    if e_ident[EI_CLASS] != ELFCLASS32 \
       and e_ident[EI_CLASS] != ELFCLASS64:
      error('{}: unsupported ELF class: {}'.format(f.name, e_ident[EI_CLASS]))

    if e_ident[EI_DATA] != ELFDATA2LSB \
       and e_ident[EI_DATA] != ELFDATA2MSB: \
      error('{}: unsupported ELF data: {}'.format(f.name, e_ident[EI_DATA]))

    ehdr = Elf_Ehdr(e_ident)
    ehdr.read(f)
    if ehdr.e_type not in (ET_EXEC, ET_DYN):
       error('{}: not an executable or shared library'.format(f.name))

    phdr = Elf_Phdr(e_ident)
    maximize_tls_size_done = False
    for i in range(0, ehdr.e_phnum):
        f.seek(ehdr.e_phoff + i * phdr.len)
        phdr.read(f)
        if phdr.p_type == PT_LOAD and opts.align is not None:
            elf_edit_align(phdr, opts.align)
            f.seek(ehdr.e_phoff + i * phdr.len)
            phdr.write(f)
            break
        if phdr.p_type == PT_TLS and opts.maximize_tls_size:
            elf_edit_maximize_tls_size(phdr, e_ident[EI_CLASS])
            f.seek(ehdr.e_phoff + i * phdr.len)
            phdr.write(f)
            maximize_tls_size_done = True
            break

    if opts.maximize_tls_size and not maximize_tls_size_done:
        error('{}: TLS maximum size was not updated'.format(f.name))

def get_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-a', dest='align',
                        help='How to set the LOAD alignment')
    parser.add_argument('--maximize-tls-size', action='store_true',
                        help='Set maximum PT_TLS size')
    parser.add_argument('output',
                        help='ELF file to edit')
    return parser


def main(argv):
    parser = get_parser()
    opts = parser.parse_args(argv)
    with open(opts.output, 'r+b') as fout:
       elf_edit(fout, opts)


if __name__ == '__main__':
    main(sys.argv[1:])
