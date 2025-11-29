libtfl_files = files(
    'tfl_device.c')

libteflon_st = static_library(
  'teflon_st',
  [libtfl_files, sha1_h],
  c_args : [  ],
  gnu_symbol_visibility : 'hidden',
  include_directories : [ inc_include, inc_src, inc_util, inc_gallium, inc_gallium_aux ],
  dependencies : [ idep_mesautil ]
)
