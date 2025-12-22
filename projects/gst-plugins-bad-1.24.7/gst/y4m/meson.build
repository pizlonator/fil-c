y4_sources = [
  'gsty4mdec.c'
]

gsty4mdec = library('gsty4mdec',
  y4_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gsty4mdec]
