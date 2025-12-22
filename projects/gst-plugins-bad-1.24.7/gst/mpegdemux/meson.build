mpegd_sources = [
  'plugin.c',
  'gstmpegdemux.c',
  'gstpesfilter.c',
]

gstmpedpsdemux = library('gstmpegpsdemux',
  mpegd_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gsttag_dep, gstpbutils_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstmpedpsdemux]
