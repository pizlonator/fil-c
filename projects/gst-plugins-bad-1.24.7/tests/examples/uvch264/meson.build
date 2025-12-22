if gtk_dep.found() and gdk_x11_dep.found()
  test_uvch264_args = [
    '-DGST_USE_UNSTABLE_API',
    cc.get_supported_link_arguments('-fvisibility=default'),
  ]
  gmodule_export_dep = dependency('gmodule-export-2.0')
  executable('test-uvch264', 'test-uvch264.c',
    include_directories : [configinc],
    dependencies : [gtk_dep, gdk_x11_dep, gst_dep, gstvideo_dep, gmodule_export_dep],
    c_args : gst_plugins_bad_args + test_uvch264_args,
    install: false)
endif
