gstaudiolatency = library('gstaudiolatency',
  'gstaudiolatency.c',
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstaudiolatency]
