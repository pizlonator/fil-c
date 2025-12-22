if have_msdk
  executable('test-roi', 'test-roi.c',
    include_directories : [configinc],
    dependencies: [gst_dep, gstvideo_dep],
    c_args : gst_plugins_bad_args,
    install: false)
endif
