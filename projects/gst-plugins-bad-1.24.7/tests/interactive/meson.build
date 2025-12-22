if not get_option('soundtouch').disabled() and soundtouch_dep.found()
  executable('pitch-test', 'pitch-test.c',
    include_directories: [configinc],
    dependencies: [gst_dep, gstcontroller_dep],
    install: false)
endif
