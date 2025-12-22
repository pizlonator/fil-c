executable('nvcodec',
  ['nvcodec.c', '../key-handler.c'],
  include_directories : [configinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  install: false)

executable('cudaipc', ['cudaipc.c'],
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  install: false)

