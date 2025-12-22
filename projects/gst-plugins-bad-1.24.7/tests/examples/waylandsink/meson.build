gtk_dep = dependency('gtk+-3.0', required : get_option('examples'))
gtk_wayland_dep = dependency('gtk+-wayland-3.0', required : get_option('examples'))

if use_wayland
  if gtk_dep.found() and gtk_wayland_dep.found()
    executable('waylandsink',
      'main.c',
      extra_files: ['window.ui'],
      install: false,
      include_directories : [configinc],
      dependencies : [gtk_dep, gtk_wayland_dep, gst_dep, gstwayland_dep],
      c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
    )
  endif
  executable('wayland_threads',
    'wayland-threads.c',
    install: false,
    include_directories : [configinc],
    dependencies : [gst_dep, gstwayland_dep, wl_client_dep],
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  )
endif
