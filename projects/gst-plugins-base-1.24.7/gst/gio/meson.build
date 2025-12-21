gio_sources = ['gstgio.c',
  'gstgioelement.c',
  'gstgiobasesink.c',
  'gstgiobasesrc.c',
  'gstgiosink.c',
  'gstgiosrc.c',
  'gstgiostreamsink.c',
  'gstgiostreamsrc.c',
]

gstgio = library('gstgio',
  gio_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [gst_base_dep, gio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstgio]
