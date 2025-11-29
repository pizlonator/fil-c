libteflon = shared_library(
  'teflon',
  [ 'target.c' ],
  include_directories : [ inc_src, inc_util, inc_include, inc_gallium, inc_gallium_aux, inc_gallium_winsys, inc_gallium_drivers ],
  link_whole : [ libteflon_st ],
  link_with : [libpipe_loader_static, libws_null, libwsw, libswdri, libswkmsdri, libgallium ],
  gnu_symbol_visibility : 'hidden',
  link_args : ld_args_build_id,
  dependencies : [
    driver_etnaviv,
    idep_nir,
  ],
  install : true,
)

if with_tests
  tensorflow_lite = shared_library(
    'tensorflow-lite',
    [ 'tflite-stub.c' ],
    include_directories : [ inc_include ],
    install : false,
  )

  tflite_flatbuffer_h_name = 'tflite-schema-v2.15.0_generated.h'
  tflite_flatbuffer_h = custom_target('tflite_flatbuffer.h',
    output: tflite_flatbuffer_h_name,
    input: 'tflite-schema-v2.15.0.fbs',
    command : [
      prog_flatc,
      '--cpp',
      '--cpp-ptr-type', 'std::shared_ptr',
      '--gen-object-api',
      '-o', meson.current_build_dir(),
      '@INPUT@'
    ],
  )

  executable(
    'test_teflon',
    'test_teflon.cpp',
    'test_executor.cpp',
    tflite_flatbuffer_h,
    dependencies : [ idep_mesautil, idep_gtest ],
    link_with : [ tensorflow_lite ],
    include_directories : [ inc_include ],
  )
endif