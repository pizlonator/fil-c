codecalpha_sources = [
  'gstplugin.c',
  'gstalphacombine.c',
  'gstalphadecodebin.c',
  'gstvp8alphadecodebin.c',
  'gstvp9alphadecodebin.c',
  'gstcodecalphademux.c',
]

gstcodecalpha = library('gstcodecalpha',
  codecalpha_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, gstpbutils_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstcodecalpha]
