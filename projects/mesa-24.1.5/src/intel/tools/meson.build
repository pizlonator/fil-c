# Copyright © 2017 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

libaub = static_library(
  'aub',
  files('aub_read.c', 'aub_mem.c'),
  include_directories : [inc_include, inc_src, inc_intel],
  dependencies : [idep_mesautil, idep_intel_dev, idep_intel_decoder_brw, idep_intel_decoder_elk],
  link_with : [libintel_common],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : false
)

aubinator = executable(
  'aubinator',
  files('aubinator.c'),
  dependencies : [idep_mesautil, dep_expat, dep_zlib, dep_dl, dep_thread, dep_m, idep_intel_dev,
                  idep_intel_decoder_brw, idep_intel_decoder_elk,
                  idep_intel_compiler_brw, idep_intel_compiler_elk],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : [libintel_common, libaub],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

aubinator_error_decode = executable(
  'aubinator_error_decode',
  files('aubinator_error_decode.c',
        'aubinator_error_decode_lib.c',
        'aubinator_error_decode_lib.h',
        'aubinator_error_decode_xe.c',
        'aubinator_error_decode_xe.h',
        'error_decode_lib.c',
        'error_decode_lib.h',
        'error_decode_xe_lib.c',
        'error_decode_xe_lib.h'),
  dependencies : [idep_mesautil, dep_zlib, dep_thread, idep_intel_dev,
                  idep_intel_decoder_brw, idep_intel_decoder_elk,
                  idep_intel_compiler_brw, idep_intel_compiler_elk],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : [libintel_common],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

error2aub = executable(
  'intel_error2aub',
  files('aub_write.h',
        'aub_write.c',
        'error2aub.c',
        'error_decode_lib.c',
        'error_decode_lib.h'),
  dependencies : [dep_zlib, dep_dl, dep_thread, dep_m, idep_intel_dev],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

error2hangdump = executable(
  'intel_error2hangdump',
  files('error2hangdump.c',
        'error2hangdump_lib.c',
        'error2hangdump_lib.h',
        'error2hangdump_xe.c',
        'error2hangdump_xe.h',
        'error_decode_lib.c',
        'error_decode_lib.h',
        'error_decode_xe_lib.c',
        'error_decode_xe_lib.h'),
  dependencies : [dep_zlib, dep_dl, dep_thread, dep_m, idep_intel_dev],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

intel_hang_replay = executable(
  'intel_hang_replay',
  files('intel_hang_replay.c'),
  dependencies : [idep_intel_dev, idep_libintel_common, dep_libdrm, dep_dl, dep_thread, dep_m,
                  idep_intel_compiler_brw, idep_intel_compiler_elk],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

sanitize_data = configuration_data()
sanitize_data.set(
  'install_libexecdir',
  join_paths(get_option('prefix'), get_option('libexecdir'))
)
sanitize_data.set(
  'install_libdir',
  join_paths(get_option('prefix'), get_option('libdir'))
)
configure_file(
  input : 'intel_sanitize_gpu.in',
  output : '@BASENAME@',
  install_dir : get_option('bindir'),
  install : true,
  configuration : sanitize_data
)

libintel_sanitize_gpu = shared_library(
  'intel_sanitize_gpu',
  files('intel_sanitize_gpu.c'),
  dependencies : [idep_mesautil, dep_dl, dep_thread, idep_intel_dev],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : libintel_common,
  c_args : [no_override_init_args, sse2_args],
  gnu_symbol_visibility : 'hidden',
  install_dir : get_option('libexecdir'),
  install : true
)

configure_file(
  input : 'intel_dump_gpu.in',
  output : '@BASENAME@',
  install_dir : get_option('bindir'),
  configuration : sanitize_data,
  install : true
)

libintel_dump_gpu = shared_library(
  'intel_dump_gpu',
  files(
    'intel_context.h', 'gfx8_context.h', 'gfx10_context.h', 'intel_aub.h',
    'aub_write.h', 'aub_write.c', 'intel_dump_gpu.c',
  ),
  dependencies : [dep_dl, idep_mesautil, idep_intel_dev],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : [libintel_common],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install_dir : get_option('libexecdir'),
  install : true
)

if with_tools.contains('drm-shim')
  configure_file(
    input : 'intel_stub_gpu.in',
    output : '@BASENAME@',
    install_dir : get_option('bindir'),
    configuration : sanitize_data,
    install : true
  )

  libintel_stub_gpu = shared_library(
    'intel_noop_drm_shim',
    ['intel_noop_drm_shim.c', intel_dev_serialize_src],
    include_directories: [inc_include, inc_src, inc_intel],
    dependencies: [dep_libdrm, dep_drm_shim, idep_mesautil, idep_libintel_common, idep_intel_dev, idep_parson],
    gnu_symbol_visibility : 'hidden',
    install : true,
  )
endif

if with_tools.contains('intel-ui')
  subdir('imgui')

  aubinator_viewer = executable(
    'aubinator_viewer',
    files('aubinator_viewer.cpp', 'aubinator_viewer_decoder.cpp'),
    dependencies : [idep_mesautil, dep_zlib, dep_dl, dep_thread, dep_m, libintel_imgui_gtk_dep, idep_intel_dev,
                    idep_intel_compiler_brw, idep_intel_compiler_elk],
    include_directories : [inc_include, inc_src, inc_intel],
    link_with : [libintel_common, libaub],
    c_args : [no_override_init_args],
    gnu_symbol_visibility : 'hidden',
    cpp_args : ['-fpermissive', '-Wno-parentheses'],
    install : true
  )

  intel_hang_viewer = executable(
    'intel_hang_viewer',
    files('intel_hang_viewer.cpp', 'aubinator_viewer_decoder.cpp'),
    dependencies : [
      idep_mesautil,
      dep_zlib,
      dep_dl,
      dep_thread,
      dep_m,
      libintel_imgui_gtk_dep,
      idep_intel_dev,
      idep_libintel_common,
      idep_intel_compiler_brw,
      idep_intel_compiler_elk,
      idep_intel_decoder_brw,
      idep_intel_decoder_elk,
    ],
    include_directories : [inc_include, inc_src, inc_intel],
    c_args : [no_override_init_args],
    gnu_symbol_visibility : 'hidden',
    cpp_args : ['-fpermissive', '-Wno-parentheses'],
    install : true
  )
endif

if with_intel_tools
  intel_dev_info = executable(
    'intel_dev_info',
    [files('intel_dev_info.c'), [intel_dev_serialize_src]],
    dependencies : [dep_libdrm, dep_dl, dep_thread, dep_m, idep_intel_dev, idep_parson, idep_nir,
                    idep_intel_compiler_brw],
    include_directories : [inc_include, inc_src, inc_intel],
    c_args : [no_override_init_args],
    gnu_symbol_visibility : 'hidden',
    install : true
  )
endif
