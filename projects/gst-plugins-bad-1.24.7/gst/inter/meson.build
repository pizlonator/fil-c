inter_sources = [
  'gstinteraudiosink.c',
  'gstinteraudiosrc.c',
  'gstintersubsink.c',
  'gstintersubsrc.c',
  'gstintervideosink.c',
  'gstintervideosrc.c',
  'gstinter.c',
  'gstintersurface.c',
]

gstinter = library('gstinter',
  inter_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, gstvideo_dep, gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstinter]
