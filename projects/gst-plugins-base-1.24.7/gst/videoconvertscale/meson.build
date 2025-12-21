videoconvertscale_sources = [
  'gstvideoconvert.c',
  'gstvideoconvertscale.c',
  'gstvideoconvertscaleplugin.c',
  'gstvideoscale.c',
]

gstvideoconvertscale = library('gstvideoconvertscale',
  videoconvertscale_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [video_dep, gst_dep, gst_base_dep],
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstvideoconvertscale]
