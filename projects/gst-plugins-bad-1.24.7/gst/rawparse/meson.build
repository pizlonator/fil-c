raw_sources = [
  'gstaudioparse.c',
  'gstvideoparse.c',
  'plugin.c',
]

gstlegacyrawparse = library('gstlegacyrawparse',
  raw_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstlegacyrawparse]
