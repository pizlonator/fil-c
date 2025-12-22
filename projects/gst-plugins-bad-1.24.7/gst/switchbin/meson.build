switchbin_sources = [
  'gstswitchbin.c', 'plugin.c'
]

gstswitchbin = library('gstswitchbin',
  switchbin_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gst_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstswitchbin]
