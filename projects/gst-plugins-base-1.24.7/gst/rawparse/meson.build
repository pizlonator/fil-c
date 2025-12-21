raw_sources = [
  'gstunalignedaudioparse.c',
  'gstunalignedvideoparse.c',
  'gstrawbaseparse.c',
  'gstrawaudioparse.c',
  'gstrawvideoparse.c',
  'plugin.c',
]

gstrawparse = library('gstrawparse',
  raw_sources,
  c_args : gst_plugins_base_args,
  include_directories : [configinc],
  dependencies : [gst_base_dep, video_dep, audio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstrawparse]

