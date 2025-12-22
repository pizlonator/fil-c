segment_sources = [
  'gstsegmentclip.c',
  'gstaudiosegmentclip.c',
  'gstvideosegmentclip.c',
  'plugin.c',
]

gstsegmentclip = library('gstsegmentclip',
  segment_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstsegmentclip]
