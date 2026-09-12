#!/usr/bin/python3
# Generate a crafted ELF with a large number of PT_NULL program headers
# for tst-bz26577.
# Copyright (C) 2026 Free Software Foundation, Inc.
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

# The generated file is an ET_DYN ELF with EVIL_PHNUM (0x7FFF) program
# headers.  The first header is a PT_LOAD covering the ELF header itself so
# the dynamic linker actually attempts to map the object and exercises the
# iterator path that replaced the old loadcmd VLA.  The remaining headers
# are PT_NULL.  The object has no PT_DYNAMIC, so dlopen and LD_PRELOAD will
# both fail gracefully after loading.

import os
import struct
import sys

import glibcelf

# Must match the definition in tst-bz26577.c.
EVIL_PHNUM = 0x7FFF

def main():
    if len(sys.argv) != 3:
        print('usage: gen-tst-bz26577-mod.py OUTPUT REF-ELF', file=sys.stderr)
        sys.exit(1)

    output_path = sys.argv[1]
    ref_elf_path = sys.argv[2]

    # Read EI_CLASS, EI_DATA and e_machine from a target ELF (ld.so) so
    # the generated file matches the target ABI, not the host Python.
    ref = glibcelf.Image.readfile(ref_elf_path)
    ei_class = ref.ehdr.e_ident.ei_class  # ElfClass.ELFCLASS32 or ELFCLASS64
    ei_data = ref.ehdr.e_ident.ei_data    # ElfData.ELFDATA2LSB or ELFDATA2MSB
    e_machine = ref.ehdr.e_machine        # Machine.*

    endian = '<' if ei_data == glibcelf.ElfData.ELFDATA2LSB else '>'
    is64 = (ei_class == glibcelf.ElfClass.ELFCLASS64)

    ehdr_size = glibcelf.Ehdr.layouts[(ei_class, ei_data)].size
    phdr_size = glibcelf.Phdr.layouts[(ei_class, ei_data)].size

    # File must hold the ELF header plus the full program header table so
    # that pread() in open_verify and _dl_map_object_from_fd can read all
    # EVIL_PHNUM entries without a short read.
    total = ehdr_size + EVIL_PHNUM * phdr_size
    # Assume workable value, the binary should be reject by the loader anyway.
    pagesize = 4096
    total = (total + pagesize - 1) & ~(pagesize - 1)

    buf = bytearray(total)

    # ELF Header:
    buf[0:4]  = b'\x7fELF'
    buf[4]    = ei_class.value
    buf[5]    = ei_data.value
    buf[6]    = 1   # EV_CURRENT
    buf[7]    = 0   # ELFOSABI_SYSV
    # bytes 8..15 remain zero (padding)

    # Pack the ELF header fields that follow e_ident.
    # ELF64 Ehdr layout (after e_ident[16]):
    #   e_type(H) e_machine(H) e_version(I)
    #   e_entry(Q) e_phoff(Q) e_shoff(Q)
    #   e_flags(I) e_ehsize(H) e_phentsize(H)
    #   e_phnum(H) e_shentsize(H) e_shnum(H) e_shstrndx(H)
    # ELF32 Ehdr layout (after e_ident[16]):
    #   e_type(H) e_machine(H) e_version(I)
    #   e_entry(I) e_phoff(I) e_shoff(I)
    #   e_flags(I) e_ehsize(H) e_phentsize(H)
    #   e_phnum(H) e_shentsize(H) e_shnum(H) e_shstrndx(H)
    if is64:
        fmt = endian + '2HI3QI6H'
    else:
        fmt = endian + '2H5I6H'

    phoff = ehdr_size   # program header table immediately follows Ehdr
    fields = (
        glibcelf.Et.ET_DYN.value,   # e_type
        e_machine.value,            # e_machine
        1,                          # e_version (EV_CURRENT)
        0,                          # e_entry
        phoff,                      # e_phoff
        0,                          # e_shoff
        0,                          # e_flags
        ehdr_size,                  # e_ehsize
        phdr_size,                  # e_phentsize
        EVIL_PHNUM,                 # e_phnum
        0,                          # e_shentsize
        0,                          # e_shnum
        0,                          # e_shstrndx
    )
    struct.pack_into(fmt, buf, 16, *fields)

    # Write the first program header as PT_LOAD covering the ELF header
    # (p_offset=0, p_filesz=ehdr_size, p_memsz=ehdr_size, PF_R). This
    # ensures the dynamic linker actually maps the segment and exercises
    # the PT_LOAD iterator path rather than aborting early with
    # "no loadable segments".  The remaining EVIL_PHNUM-1 headers stay
    # zero (PT_NULL).
    #
    # ELF64 Phdr field order (layout '2I6Q'):
    #   p_type(I) p_flags(I) p_offset(Q) p_vaddr(Q) p_paddr(Q)
    #   p_filesz(Q) p_memsz(Q) p_align(Q)
    # ELF32 Phdr field order (layout '8I'):
    #   p_type(I) p_offset(I) p_vaddr(I) p_paddr(I)
    #   p_filesz(I) p_memsz(I) p_flags(I) p_align(I)
    if is64:
        phdr_fmt = endian + '2I6Q'
        phdr_fields = (
            glibcelf.Pt.PT_LOAD.value,   # p_type
            glibcelf.Pf.PF_R.value,      # p_flags
            0,                           # p_offset
            0,                           # p_vaddr
            0,                           # p_paddr
            ehdr_size,                   # p_filesz
            ehdr_size,                   # p_memsz
            pagesize,                    # p_align
        )
    else:
        phdr_fmt = endian + '8I'
        phdr_fields = (
            glibcelf.Pt.PT_LOAD.value,   # p_type
            0,                           # p_offset
            0,                           # p_vaddr
            0,                           # p_paddr
            ehdr_size,                   # p_filesz
            ehdr_size,                   # p_memsz
            glibcelf.Pf.PF_R.value,      # p_flags
            pagesize,                    # p_align
        )
    struct.pack_into(phdr_fmt, buf, ehdr_size, *phdr_fields)

    with open(output_path, 'wb') as f:
        f.write(buf)

if __name__ == '__main__':
    main()
