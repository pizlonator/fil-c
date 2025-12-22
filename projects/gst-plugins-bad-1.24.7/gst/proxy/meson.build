proxy_sources = [
  'gstproxy.c',
  'gstproxysink.c',
  'gstproxysrc.c'
]

gstproxy = library('gstproxy',
  proxy_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstproxy]