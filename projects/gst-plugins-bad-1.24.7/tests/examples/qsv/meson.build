if host_system not in ['windows', 'linux']
  subdir_done()
endif

executable('qsvenc-dynamic-reconfigure',
  ['qsvenc-dynamic-reconfigure.c', '../key-handler.c'],
  include_directories : [configinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  c_args : gst_plugins_bad_args,
  install: false)
