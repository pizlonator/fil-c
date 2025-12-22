gdp_sources = [
  'dataprotocol.c',
  'gstgdpelement.c',
  'gstgdp.c',
  'gstgdppay.c',
  'gstgdpdepay.c',
]

gstdgp = library('gstgdp',
  gdp_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstdgp]
