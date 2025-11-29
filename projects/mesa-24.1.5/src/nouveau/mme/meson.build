# Copyright © 2022 Collabora, Ltd.
# SPDX-License-Identifier: MIT

libnouveau_mme_files = files(
  'mme_builder.h',
  'mme_fermi.c',
  'mme_fermi.h',
  'mme_fermi_builder.c',
  'mme_fermi_sim.c',
  'mme_tu104.c',
  'mme_tu104.h',
  'mme_tu104_builder.c',
  'mme_tu104_sim.c',
  'mme_tu104_sim.h',
)

_libnouveau_mme = static_library(
  'nouveau_mme',
  libnouveau_mme_files,
  include_directories : [inc_include, inc_src],
  gnu_symbol_visibility : 'hidden',
  dependencies : [
    idep_mesautil,
    idep_nvidia_headers,
  ],
)

idep_nouveau_mme = declare_dependency(
  include_directories : include_directories('.'),
  link_with : _libnouveau_mme,
)

if with_tests and not with_platform_android
  test('mme_builder',
    executable(
      'mme_builder_test',
      files('tests/mme_runner.cpp', 'tests/mme_builder_test.cpp'),
      gnu_symbol_visibility : 'hidden',
      include_directories : [inc_include, inc_src],
      dependencies : [
        dep_libdrm,
        idep_gtest,
        idep_mesautil,
        idep_nvidia_headers,
        idep_nouveau_mme,
        idep_nouveau_ws
      ],
    ),
    suite : ['nouveau'],
  )

  executable(
    'mme_fermi_sim_hw_test',
    files('tests/mme_runner.cpp', 'tests/mme_fermi_sim_hw_test.cpp'),
    gnu_symbol_visibility : 'hidden',
    include_directories : [inc_include, inc_src],
    dependencies : [
      dep_libdrm,
      idep_gtest,
      idep_mesautil,
      idep_nvidia_headers,
      idep_nouveau_mme,
      idep_nouveau_ws
    ],
    install : true,
  )

  executable(
    'mme_tu104_sim_hw_test',
    files('tests/mme_runner.cpp', 'tests/mme_tu104_sim_hw_test.cpp'),
    gnu_symbol_visibility : 'hidden',
    include_directories : [inc_include, inc_src],
    dependencies : [
      dep_libdrm,
      idep_gtest,
      idep_mesautil,
      idep_nvidia_headers,
      idep_nouveau_mme,
      idep_nouveau_ws
    ],
    install : true,
  )
endif

executable(
  'nv_mme_dump',
  'nv_mme_dump.c',
  dependencies : [ idep_mesautil, idep_nouveau_mme ],
  build_by_default : with_tools.contains('nouveau'),
  install : with_tools.contains('nouveau'),
)
