zxing_sources = [
  'gstzxing.cpp',
  'gstzxingplugin.c',
]
zxing_dep = dependency('zxing', version : '>= 1.4.0', required : get_option('zxing'))
if zxing_dep.found()
	gstzxing = library('gstzxing',
	  zxing_sources,
	  c_args : gst_plugins_bad_args,
	  cpp_args: '-DZX_USE_UTF8',
	  include_directories : [configinc],
	  dependencies : [gstvideo_dep, zxing_dep],
	  install : true,
	  install_dir : plugins_install_dir,
	  override_options : ['cpp_std=c++17'],
	)
	plugins += [gstzxing]
endif
