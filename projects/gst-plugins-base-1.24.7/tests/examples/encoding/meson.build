executable('encoding', 'encoding.c', 'gstcapslist.c',
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [gst_dep, pbutils_dep, tag_dep, video_dep, audio_dep],
  install: false)
