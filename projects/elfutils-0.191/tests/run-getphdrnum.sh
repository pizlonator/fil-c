#! /bin/sh

# testfile-phdrs.elf generated with python3 script
# import struct
# import sys
# 
# phnum = 66000
# 
# sys.stdout.buffer.write(
#     struct.pack(
#         "<16BHHIQQQIHHHHHH",
#         # EI_MAG
#         *b"\x7fELF",
#         # EI_CLASS = ELFCLASS64
#         2,
#         # EI_DATA = ELFDATA2LSB
#         1,
#         # EI_VERSION
#         1,
#         # EI_OSABI = ELFOSABI_SYSV
#         0,
#         # EI_ABIVERSION
#         0,
#         # EI_PAD
#         *bytes(7),
#         # e_type = ET_CORE
#         4,
#         # e_machine = EM_X86_64
#         62,
#         # e_version
#         1,
#         # e_entry
#         0,
#         # e_phoff = sizeof(Elf64_Ehdr) + sizeof(Elf64_Shdr)
#         128,
#         # e_shoff = sizeof(Elf64_Ehdr)
#         64,
#         # e_flags
#         0,
#         # e_ehsize
#         64,
#         # e_phentsize
#         56,
#         # e_phnum = PN_XNUM
#         0xFFFF,
#         # e_shentsize
#         64,
#         # e_shnum
#         1,
#         # e_shstrndx
#         0,
#     )
# )
# 
# sys.stdout.buffer.write(
#     struct.pack(
#         "<IIQQQQIIQQ",
#         # sh_name
#         0,
#         # sh_type = SHT_NULL
#         0,
#         # sh_flags
#         0,
#         # sh_addr
#         0,
#         # sh_offset
#         0,
#         # sh_size
#         0,
#         # sh_link
#         0,
#         # sh_info
#         phnum,
#         # sh_addralign
#         0,
#         # sh_entsize
#         0,
#     )
# )
# 
# for i in range(phnum):
#     sys.stdout.buffer.write(
#         struct.pack(
#             "<IIQQQQQQ",
#             # p_type = PT_LOAD
#             1,
#             # p_flags = PF_X|PF_W|PF_R
#             0x7,
#             # p_offset
#             0,
#             # p_vaddr
#             # i * 4096,
#             4096,
#             # p_paddr
#             0,
#             # p_filesz
#             0,
#             # p_memsz
#             4096,
#             # p_align
#             0,
#         )
#     )

. $srcdir/test-subr.sh

testfiles testfile-phdrs.elf

testrun_compare ${abs_top_builddir}/src/readelf -h testfile-phdrs.elf<<\EOF
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Ident Version:                     1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              CORE (Core file)
  Machine:                           AMD x86-64
  Version:                           1 (current)
  Entry point address:               0
  Start of program headers:          128 (bytes into file)
  Start of section headers:          64 (bytes into file)
  Flags:                             
  Size of this header:               64 (bytes)
  Size of program header entries:    56 (bytes)
  Number of program headers entries: 65535 (66000 in [0].sh_info)
  Size of section header entries:    64 (bytes)
  Number of section headers entries: 1
  Section header string table index: 0

EOF

testrun_compare ${abs_builddir}/getphdrnum testfile-phdrs.elf<<\EOF
66000
EOF

exit 0
