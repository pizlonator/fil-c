gstdebug = library('gstbasedebug',
  'gstdebug.c',
  'gstfakevideodec.c',
  c_args: gst_plugins_base_args,
  include_directories : [configinc],
  dependencies : [gst_dep, gst_base_dep, video_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstdebug]

