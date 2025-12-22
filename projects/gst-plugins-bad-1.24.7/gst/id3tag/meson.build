id3_sources = [
  'gstid3mux.c',
  'id3tag.c',
]

gstid3tag = library('gstid3tag',
  id3_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gsttag_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstid3tag]
