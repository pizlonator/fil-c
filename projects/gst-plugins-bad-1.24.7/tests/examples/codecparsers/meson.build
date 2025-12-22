executable('parse-jpeg', 'parse-jpeg.c',
  include_directories : [configinc],
  dependencies : [gstcodecparsers_dep, gst_dep],
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  install: false)

executable('parse-vp8', 'parse-vp8.c',
  include_directories : [configinc],
  dependencies : [gstcodecparsers_dep, gst_dep],
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  install: false)
