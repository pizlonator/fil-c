subover_sources = [
  'dvb-sub.c',
  'gstdvbsuboverlay.c',
]

gstdvbsuboverlay = library('gstdvbsuboverlay',
  subover_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstdvbsuboverlay]
