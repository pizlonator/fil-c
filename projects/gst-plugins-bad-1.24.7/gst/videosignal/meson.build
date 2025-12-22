vsignal_sources = [
  'gstvideosignal.c',
  'gstvideoanalyse.c',
  'gstsimplevideomarkdetect.c',
  'gstsimplevideomark.c',
]

gstvideosignal = library('gstvideosignal',
  vsignal_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstvideosignal]
