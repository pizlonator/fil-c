#
# Copyright © 2024 Igalia S.L.
# SPDX-License-Identifier: MIT
#

has_pycparser = run_command(
  prog_python, '-c',
  '''
try:
  from packaging.version import Version
except:
  from distutils.version import StrictVersion as Version
import pycparser
assert Version(pycparser.__version__) >= Version("2.20")
  ''', check: false)
if has_pycparser.returncode() != 0
  error('Python (3.x) pycparser module >= 2.20 required to build mesa.')
endif

prog_hwdb = files('hwdb.h.py')
hwdb_h = custom_target(
  'hwdb.h',
  input: [
    'amlogic/gc_feature_database.h',
    'nxp/gc_feature_database.h',
    'st/gc_feature_database.h'
  ],
  output: 'hwdb.h',
  command: [prog_python, prog_hwdb, '--output', '@OUTPUT@', '@INPUT@'],
)

libetnaviv_hwdb_files = [
  hwdb_h,
  'etna_hwdb.c',
]

libetnaviv_hwdb = static_library(
  'etnaviv_hwdb',
  libetnaviv_hwdb_files,
  dependencies: idep_etna_common,
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  build_by_default : false,
)
