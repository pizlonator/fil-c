if not add_languages('cpp', native: false, required: get_option('modplug'))
  subdir_done()
endif

modplug_dep = dependency('libmodplug', required: get_option('modplug'))

if modplug_dep.found()
  gstmodplug = library('gstmodplug',
    'gstmodplug.cc',
    cpp_args: gst_plugins_bad_args,
    include_directories: [configinc],
    dependencies: [gstaudio_dep, modplug_dep, libm],
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstmodplug]
endif
