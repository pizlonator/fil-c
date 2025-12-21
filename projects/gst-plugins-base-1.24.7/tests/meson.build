if get_option('tests').disabled() or static_build
  subdir_done()
endif

pluginsdirs = []
if gst_dep.type_name() == 'pkgconfig'
  pluginsdirs = [gst_dep.get_variable('pluginsdir')]
  gst_plugin_scanner_dir = gst_dep.get_variable('pluginscannerdir')
else
  gst_plugin_scanner_dir = gst_proj.get_variable('gst_scanner_dir')
endif
gst_plugin_scanner_path = join_paths(gst_plugin_scanner_dir, 'gst-plugin-scanner')

if gst_check_dep.found()
  subdir('check')
  subdir('interactive')
  subdir('validate')
endif
if not get_option('examples').disabled()
  subdir('examples')
endif
