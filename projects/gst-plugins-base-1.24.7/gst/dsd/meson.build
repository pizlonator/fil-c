dsd_sources = [
  'gstdsdconvert.c',
  'plugin.c'
  ]

gstdsd = library('gstdsd',
  dsd_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [audio_dep, gst_base_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstdsd]
