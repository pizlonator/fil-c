rfbsrc_sources = [
  'gstrfbsrc.c',
  'gstrfb-utils.c',
  'rfbdecoder.c',
  'd3des.c',
]

librfb_incs = include_directories('..')

gstrfbsrc = library('gstrfbsrc',
  rfbsrc_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc, libsinc, librfb_incs],
  dependencies : [gstbase_dep, gstvideo_dep, gio_dep, x11_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstrfbsrc]
