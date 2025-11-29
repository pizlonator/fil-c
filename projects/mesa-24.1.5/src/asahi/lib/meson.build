# Copyright 2018 Rob Clark
# Copyright 2019 Collabora, Ltd.
# Copyright 2017 Intel Corporation
# SPDX-License-Identifier: MIT

dep_iokit = dependency('IOKit', required : false)

libasahi_lib_files = files(
  'agx_bo.c',
  'agx_border.c',
  'agx_device.c',
  'agx_formats.c',
  'agx_linker.c',
  'agx_meta.c',
  'agx_tilebuffer.c',
  'agx_nir_lower_alpha.c',
  'agx_nir_lower_gs.c',
  'agx_nir_lower_ia.c',
  'agx_nir_lower_msaa.c',
  'agx_nir_lower_sample_intrinsics.c',
  'agx_nir_lower_tess.c',
  'agx_nir_lower_texture.c',
  'agx_nir_lower_tilebuffer.c',
  'agx_nir_lower_uvs.c',
  'agx_nir_lower_vbo.c',
  'agx_nir_prolog_epilog.c',
  'agx_ppp.h',
  'agx_scratch.c',
  'pool.c',
)

libasahi_decode_files = files(
  'decode.c',
)

libagx_shader_files = files(
  'shaders/libagx.h',
  'shaders/geometry.cl',
  'shaders/geometry.h',
  'shaders/tessellation.cl',
  'shaders/tessellator.cl',
  'shaders/texture.cl',
  'shaders/helper.cl',
)

libasahi_decode = static_library(
  'asahi_decode',
  [libasahi_decode_files, agx_pack],
  dependencies : [dep_valgrind, idep_mesautil],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  build_by_default : false,
)

libasahi_decode_shared = shared_library(
  'asahi_decode',
  [libasahi_decode_files, agx_pack],
  dependencies : [dep_valgrind, idep_mesautil],
  c_args : [no_override_init_args],
  build_by_default : with_tools.contains('asahi'),
)

prepended_input_args = []
foreach input_arg : libagx_shader_files
  prepended_input_args += ['--in', input_arg]
endforeach
libagx_shaders = custom_target(
  'libagx_shaders.h',
  input : libagx_shader_files,
  output : 'libagx_shaders.h',
  command : [
    prog_asahi_clc, '--prefix', 'libagx_shaders',
    prepended_input_args, '-o', '@OUTPUT@', '--',
    '-cl-std=cl2.0', '-D__OPENCL_VERSION__=200',
    '-I' + join_paths(meson.current_source_dir(), '.'),
    '-I' + join_paths(meson.current_source_dir(), '../../'),
    '-I' + join_paths(meson.current_source_dir(), 'shaders'),
    '-I' + join_paths(meson.current_build_dir(), '../genxml'),
    ],
  env: ['MESA_SHADER_CACHE_DISABLE=true'],
  depends : [prog_asahi_clc, agx_pack],
)

libasahi_lib = static_library(
  'asahi_lib',
  [libasahi_lib_files, libagx_shaders, agx_pack],
  include_directories : inc_asahi,
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  link_with: [libasahi_decode],
  dependencies: [dep_libdrm, dep_valgrind, idep_nir],
  build_by_default : false,
)

if with_tests
  test(
    'libasahi_tests',
    executable(
      'libasahi_tests',
      files(
        'tests/test-packing.cpp',
        'tests/test-tilebuffer.cpp',
      ),
      c_args : [c_msvc_compat_args, no_override_init_args],
      gnu_symbol_visibility : 'hidden',
      dependencies: [idep_gtest, idep_agx_pack, idep_mesautil],
      link_with : [libasahi_lib],
    ),
    suite : ['asahi'],
    protocol : 'gtest',
  )
endif

if dep_iokit.found()
  libasahi_wrap = shared_library(
    'wrap',
    'wrap.c',
    include_directories : [inc_src, inc_asahi],
    dependencies : [idep_mesautil, dep_iokit],
    c_args : [no_override_init_args, '-Wno-missing-prototypes'],
    gnu_symbol_visibility : 'hidden',
    build_by_default : with_tools.contains('asahi'),
    link_with: libasahi_decode,
  )
endif
