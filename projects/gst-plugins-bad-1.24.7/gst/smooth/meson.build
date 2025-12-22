smooth_sources = [
  'gstsmooth.c',
]

gstsmooth = library('gstsmooth',
  smooth_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstsmooth]
