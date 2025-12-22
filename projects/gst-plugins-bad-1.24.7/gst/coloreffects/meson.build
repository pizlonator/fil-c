coloreffects_sources = [
  'gstplugin.c',
  'gstcoloreffects.c',
  'gstchromahold.c',
]

gstcoloreffects = library('gstcoloreffects',
  coloreffects_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstcoloreffects]
