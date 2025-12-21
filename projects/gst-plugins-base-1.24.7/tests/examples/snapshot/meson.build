if gdk_pixbuf_dep.found()
  executable('snapshot', 'snapshot.c',
    c_args : gst_plugins_base_args,
    include_directories: [configinc, libsinc],
    dependencies : [libm, gst_dep, video_dep, gdk_pixbuf_dep],
    install: false)
endif
