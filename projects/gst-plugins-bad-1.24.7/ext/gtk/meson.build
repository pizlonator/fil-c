gtkwayland_sources = [
  'gstplugin.c',
  'gstgtkutils.c',
  'gstgtkwaylandsink.c',
  'gtkgstbasewidget.c',
  'gtkgstwaylandwidget.c',
]

gtk_dep = dependency('gtk+-3.0', required : get_option('gtk3'))
gtk_wayland_dep = dependency('gtk+-wayland-3.0', required : get_option('gtk3'))

if gtk_dep.found() and gtk_wayland_dep.found() and use_wayland
  gstgtkwayland = library('gstgtkwayland',
    gtkwayland_sources,
    c_args :  gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
    include_directories : [configinc],
    dependencies : [gtk_dep, gstvideo_dep, gstwayland_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstgtkwayland]
endif
