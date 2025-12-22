jp2_sources = [
  'gstjp2kdecimator.c',
  'jp2kcodestream.c',
]

gstjp2kdecimator = library('gstjp2kdecimator',
  jp2_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstjp2kdecimator]
