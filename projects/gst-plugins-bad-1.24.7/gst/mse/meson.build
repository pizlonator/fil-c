mse_sources = ['gstmse.c']

gstmse_plugin = library('gstmse',
  mse_sources,
  c_args: gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  include_directories: [configinc],
  dependencies : [gstbase_dep, gstmse_dep],
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstmse_plugin]
