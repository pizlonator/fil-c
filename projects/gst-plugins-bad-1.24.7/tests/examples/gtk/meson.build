if gtk_dep.found() and gtk_wayland_dep.found() and use_wayland
  executable('gtkwaylandsink',
    'gtkwaylandsink.c',
    extra_files: ['window.ui'],
    install: false,
    include_directories : [configinc],
    dependencies : [gtk_dep, gst_dep],
    c_args : gst_plugins_bad_args,
  )
endif
