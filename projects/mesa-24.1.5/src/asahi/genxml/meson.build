# Copyright 2017 Intel Corporation
# SPDX-License-Identifier: MIT

agx_pack = custom_target(
  'agx_pack.h',
  input : ['gen_pack.py', 'cmdbuf.xml'],
  output : 'agx_pack.h',
  command : [prog_python, '@INPUT@'],
  capture : true,
)

idep_agx_pack = declare_dependency(
  sources : [agx_pack],
  dependencies : dep_valgrind,
  include_directories : include_directories('.'),
)


