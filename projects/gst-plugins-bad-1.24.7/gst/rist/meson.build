rist_sources = [
  'gstroundrobin.c',
  'gstristrtxsend.c',
  'gstristrtxreceive.c',
  'gstristsrc.c',
  'gstristsink.c',
  'gstrist.c',
  'gstristplugin.c',
  'gstristrtpext.c',
  'gstristrtpdeext.c'
]

gstrist = library('gstrist',
  rist_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstrtp_dep, gstnet_dep, gio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstrist]
