if gtk_dep.found()
  executable('giosrc-mounting', 'giosrc-mounting.c',
    c_args : gst_plugins_base_args,
    include_directories: [configinc, libsinc],
    dependencies : [gst_dep, gtk_dep, gio_dep],
    install: false)
endif
