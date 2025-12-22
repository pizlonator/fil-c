silence_sources = [
  'gstremovesilence.c',
  'vad_private.c',
]

gstremovesilence = library('gstremovesilence',
  silence_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstremovesilence]
