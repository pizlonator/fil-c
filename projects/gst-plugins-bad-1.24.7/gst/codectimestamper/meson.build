codectimestamper_sources = [
  'gstcodectimestamper.c',
  'gsth264timestamper.c',
  'gsth265timestamper.c',
  'plugin.c',
]

gstcodectimestamper = library('gstcodectimestamper',
  codectimestamper_sources,
  c_args : gst_plugins_bad_args + [ '-DGST_USE_UNSTABLE_API' ],
  include_directories : [configinc],
  dependencies : [gstcodecparsers_dep, gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstcodectimestamper]
