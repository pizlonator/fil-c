autocon_sources = [
  'gstbaseautoconvert.c',
  'gstautoconvert.c',
  'gstautovideo.c',
  'gstautovideoconvert.c',
  'gstautodeinterlace.c',
  'gstautovideoflip.c',
  'plugin.c',
]

gstautoconvert = library('gstautoconvert',
  autocon_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstpbutils_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstautoconvert]
