app_sources = [
  'gstapp.c',
  'gstappsink.c',
  'gstappsrc.c',
]

gstapp_plugin = library('gstapp',
  app_sources,
  c_args: gst_plugins_base_args,
  include_directories: [configinc],
  dependencies : [gst_base_dep, app_dep, tag_dep],
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstapp_plugin]
