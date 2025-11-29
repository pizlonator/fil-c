# Copyright © 2017 Dylan Baker
# Copyright © 2018 Intel Corporation
# SPDX-License-Identifier: MIT

# TODO: support non-static targets

nine_version = ['1', '0', '0']

gallium_nine_c_args = []
gallium_nine_ld_args = []
gallium_nine_link_depends = []
gallium_nine_link_with = [
    libgallium, libnine_st,
    libpipe_loader_static, libws_null, libwsw, libswdri,
    libswkmsdri, libgalliumvl_stub,
]

if with_ld_version_script
  gallium_nine_ld_args += ['-Wl,--version-script', join_paths(meson.current_source_dir(), 'd3dadapter9.sym')]
  gallium_nine_link_depends += files('d3dadapter9.sym')
endif

libgallium_nine = shared_library(
  'd3dadapter9',
  files('description.c', 'getproc.c', 'drm.c'),
  include_directories : [
    inc_include, inc_src, inc_loader, inc_mapi, inc_mesa, inc_util,
    inc_gallium, inc_gallium_aux, inc_gallium_winsys,
    inc_gallium_drivers, inc_d3d9,
    include_directories('../../frontends/nine'),
  ],
  c_args : [gallium_nine_c_args],
  gnu_symbol_visibility : 'hidden',
  link_args : [ld_args_build_id, ld_args_gc_sections, gallium_nine_ld_args],
  link_depends : gallium_nine_link_depends,
  link_with : gallium_nine_link_with,
  dependencies : [
    dep_selinux, dep_libdrm, dep_llvm, dep_thread,
    idep_xmlconfig, idep_mesautil, idep_nir,
    driver_asahi,
    driver_v3d,
    driver_crocus,
    driver_etnaviv,
    driver_freedreno,
    driver_kmsro,
    driver_lima,
    driver_nouveau,
    driver_panfrost,
    driver_r300,
    driver_r600,
    driver_radeonsi,
    driver_svga,
    driver_swrast,
    driver_i915,
    driver_iris,
    driver_v3d,
    driver_vc4,
    driver_zink
  ],
  name_prefix : '',
  version : '.'.join(nine_version),
  install : true,
  install_dir : d3d_drivers_path,
)

pkg.generate(
  name : 'd3d',
  description : 'Native D3D driver modules',
  version : '.'.join(nine_version),
  requires_private : 'libdrm >= ' + dep_libdrm.version(),
  variables : ['moduledir=@0@'.format(d3d_drivers_path)],
)
