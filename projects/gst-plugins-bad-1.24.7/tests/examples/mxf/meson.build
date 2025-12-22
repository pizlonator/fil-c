if gtk_dep.found()
  executable('mxfdemux-structure', 'mxfdemux-structure.c',
    include_directories : [configinc],
    dependencies: [gst_dep, gtk_dep],
    c_args : gst_plugins_bad_args,
    install: false)
endif
