frei0r_sources = [
  'gstfrei0r.c',
  'gstfrei0rfilter.c',
  'gstfrei0rsrc.c',
  'gstfrei0rmixer.c',
]

gstfrei0r = library('gstfrei0r',
  frei0r_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, gmodule_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstfrei0r]
