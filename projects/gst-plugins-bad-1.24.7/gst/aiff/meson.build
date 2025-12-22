aiff_sources = [
  'aiff.c', 'aiffmux.c', 'aiffparse.c', 'gstaiffelement.c'
]

gstaiff = library('gstaiff',
  aiff_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstbase_dep, gsttag_dep, gstaudio_dep, gstpbutils_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstaiff]
