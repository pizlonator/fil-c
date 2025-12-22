if not add_languages('cpp', native: false, required: get_option('openni2'))
  subdir_done()
endif

openni2_dep = dependency('libopenni2', version: '>= 0.26', required: get_option('openni2'))
if openni2_dep.found()
  gstopenni2 = library('gstopenni2',
    'gstopenni2.cpp', 'gstopenni2src.cpp',
    cpp_args: gst_plugins_bad_args,
    link_args: noseh_link_args,
    include_directories: [configinc],
    dependencies: [gstvideo_dep, openni2_dep],
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstopenni2]
endif
