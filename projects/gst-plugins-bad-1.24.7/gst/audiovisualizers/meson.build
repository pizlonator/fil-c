audiovis_sources = [
  'plugin.c',
  'gstspacescope.c',
  'gstspectrascope.c',
  'gstsynaescope.c',
  'gstwavescope.c',
]

gstaudiovisualizers = library('gstaudiovisualizers',
  audiovis_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstpbutils_dep, gstaudio_dep, gstvideo_dep, gstfft_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstaudiovisualizers]
