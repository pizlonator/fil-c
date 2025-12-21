gstpbtypes_sources = [
  'gstpbtypes.c'
  ]

gstpbtypes = library('gstpbtypes',
  gstpbtypes_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [video_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstpbtypes]
