audiobuffersplit_sources = [
  'gstaudiobuffersplit.c',
]

gstaudiobuffersplit = library('gstaudiobuffersplit',
  audiobuffersplit_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstaudiobuffersplit]
