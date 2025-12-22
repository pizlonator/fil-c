pnm_sources = [
  'gstpnmutils.c',
  'gstpnm.c',
  'gstpnmdec.c',
  'gstpnmenc.c',
]

gstpnm = library('gstpnm',
  pnm_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstpnm]
