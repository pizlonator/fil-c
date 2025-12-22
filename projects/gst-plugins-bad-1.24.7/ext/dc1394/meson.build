dc1394_dep = dependency('libdc1394-2', version: '>= 2.2.5', required: get_option('dc1394'))
if dc1394_dep.found()
  gstdc1394 = library('gstdc1394',
    'gstdc1394src.c',
    c_args: gst_plugins_bad_args,
    include_directories: [configinc],
    dependencies: [gstvideo_dep, dc1394_dep],
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstdc1394]
endif
