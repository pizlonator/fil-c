insertbin_sources = [
  'plugin.c'
]

gstinsertbin = library('gstinsertbin',
  insertbin_sources,
  c_args : gst_plugins_bad_args + [ '-DGST_USE_UNSTABLE_API' ],
  include_directories : [configinc],
  dependencies : [gst_dep, gstinsertbin_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstinsertbin]
