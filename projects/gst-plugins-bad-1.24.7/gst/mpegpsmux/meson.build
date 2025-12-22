psmux_sources = [
  'mpegpsmux.c',
  'psmux.c',
  'psmuxstream.c',
  'mpegpsmux_aac.c',
  'mpegpsmux_h264.c',
]

gstmpegpsmux = library('gstmpegpsmux',
  psmux_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstmpegpsmux]
