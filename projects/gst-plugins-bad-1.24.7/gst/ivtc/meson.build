ivtc_sources = [
  'gstivtc.c',
  'gstcombdetect.c',
]

gstivtc = library('gstivtc',
  ivtc_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstivtc]
