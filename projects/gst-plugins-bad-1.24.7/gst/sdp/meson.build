sdp_sources = [
  'gstsdpelem.c',
  'gstsdpdemux.h',
  'gstsdpdemux.c',
  'gstsdpsrc.c'
]

gstsdpelem = library('gstsdpelem',
  sdp_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstrtp_dep, gstsdp_dep, gio_dep, gstapp_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstsdpelem]
