#
# Copyright © 2023 Igalia S.L.
# SPDX-License-Identifier: MIT
#

isa_depend_files = [
  'etnaviv.xml',
  isaspec_py_deps,
]

etnaviv_isa = custom_target(
  'etnaviv-isa',
  input: ['etnaviv.xml'],
  output: ['etnaviv-isa.c', 'etnaviv-isa.h'],
  command: [
    prog_isaspec_decode, '--xml', '@INPUT@',
    '--out-c', '@OUTPUT0@', '--out-h', '@OUTPUT1@',
  ],
  depend_files: isa_depend_files,
)

decode_files = [
  etnaviv_isa,
  'isa.h',
]

_libetnaviv_decode = static_library(
  'etnaviv_decode',
  decode_files,
  dependencies: [idep_mesautil, idep_isaspec_decode],
  link_with: [libisaspec],
  include_directories: [
    inc_include,
    inc_src,
  ],
  gnu_symbol_visibility: 'hidden',
)

idep_libetnaviv_decode = declare_dependency(
  include_directories : [
    include_directories('.'),
    inc_include,
    inc_src
  ],
  link_with : _libetnaviv_decode,
  dependencies: [idep_isaspec_decode],
  sources : [etnaviv_isa],
)

enums_h = custom_target(
  'enums.h',
  input: ['enums.h.py', 'etnaviv.xml'],
  output: 'enums.h',
  command: [prog_python, '@INPUT@'],
  capture: true,
  depend_files: isa_depend_files,
)

encode_h = custom_target(
  'encode.h',
  input: ['etnaviv.xml'],
  output: 'encode.h',
  command: [
    prog_isaspec_encode, '--xml', '@INPUT@', '--out-h', '@OUTPUT@'
  ],
  depend_files: isa_depend_files,
)

encode_files = [
  enums_h,
  encode_h,
  'encode.c',
]

libetnaviv_encode = static_library(
  'etnaviv_encode',
  encode_files,
  gnu_symbol_visibility: 'hidden',
  include_directories: [
    inc_src,
  ],
)

disasm = executable(
  'etnaviv-disasm',
  ['disasm.c'],
  dependencies: idep_libetnaviv_decode,
  build_by_default: with_tools.contains('etnaviv'),
  include_directories: [
    inc_src,
  ],
  install: false,
)

subdir('tests')
