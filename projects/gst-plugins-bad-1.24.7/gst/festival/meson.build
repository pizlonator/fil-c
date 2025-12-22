festival_sources = [
  'gstfestival.c'
]

gstfestival = library('gstfestival',
  festival_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep] + winsock2 + network_deps,
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstfestival]
