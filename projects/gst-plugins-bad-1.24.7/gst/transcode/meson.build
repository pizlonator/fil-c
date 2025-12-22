gsttranscoder_plugin = library('gsttranscode',
  'gsttranscodeelement.c',
  'gsttranscodeplugin.c',
  'gsttranscodebin.c',
  'gst-cpu-throttling-clock.c',
  'gsturitranscodebin.c',
  install : true,
  dependencies : [gst_dep, gstpbutils_dep],
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  install_dir : plugins_install_dir,
)

plugins += [gsttranscoder_plugin]
