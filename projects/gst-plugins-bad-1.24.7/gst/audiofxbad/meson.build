audiofxbad_sources = [
  'gstaudiofxbad.c',
  'gstaudiochannelmix.c',
]

gstaudiofxbad = library('gstaudiofxbad',
  audiofxbad_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstaudiofxbad]
