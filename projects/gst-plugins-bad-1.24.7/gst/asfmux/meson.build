asfmux_sources = [
  'gstasfmux.c',
  'gstasfobjects.c',
  'gstasfparse.c',
  'gstrtpasfpay.c',
  'gstasf.c',
]

gstasfmux = library('gstasfmux',
  asfmux_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstbase_dep, gstrtp_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstasfmux]
