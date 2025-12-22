if host_system not in ['windows', 'linux']
  subdir_done()
endif

executable('decoder-caps-update',
  ['decoder-caps-update.cpp', '../key-handler.c'],
  include_directories : [configinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  c_args : gst_plugins_bad_args,
  install: false)
