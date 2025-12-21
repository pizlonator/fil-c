encoding_sources = [
  'gstencodebasebin.c',
  'gstencodebin.c',
  'gstencodebin2.c',
  'gstsmartencoder.c',
  'gststreamcombiner.c',
  'gststreamsplitter.c',
  'plugin.c',
  'gstencodingelements.c',
]

gstencoding = library('gstencoding',
  encoding_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [pbutils_dep, video_dep, gst_base_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstencoding]
