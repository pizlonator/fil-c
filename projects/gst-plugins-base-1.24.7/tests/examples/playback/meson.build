if gtk_dep.found()
  if gtk_quartz_dep.found()
    quartz_cflags = ['-xobjective-c']
  else
    quartz_cflags = []
  endif

  executable('playback-test', 'playback-test.c',
    c_args : gst_plugins_base_args + quartz_cflags,
    include_directories: [configinc, libsinc],
    dependencies : [gst_dep, video_dep, libm, gtk_dep],
    install: false)
endif
