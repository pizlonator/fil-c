dvdspu_sources = [
  'gstdvdspu.c',
  'gstspu-vobsub.c',
  'gstspu-vobsub-render.c',
  'gstspu-pgs.c',
]

gstdvdspu = library('gstdvdspu',
  dvdspu_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstdvdspu]
