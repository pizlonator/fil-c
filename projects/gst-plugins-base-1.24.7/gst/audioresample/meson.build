resample_sources = ['gstaudioresample.c']

audioresample = library('gstaudioresample',
  resample_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [audio_dep, gst_base_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [audioresample]
