gstadpcmenc_sources = [
  'adpcmenc.c'
]

gstadpcmenc = library('gstadpcmenc',
  gstadpcmenc_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstadpcmenc]
