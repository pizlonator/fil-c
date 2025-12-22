if host_system != 'windows'
  subdir_done()
endif

executable('d3d12enc-dynamic-reconfigure',
  ['d3d12enc-dynamic-reconfigure.c', '../key-handler.c'],
  include_directories : [configinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  c_args : gst_plugins_bad_args,
  install: false)
