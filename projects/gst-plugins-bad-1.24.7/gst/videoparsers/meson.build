vparse_sources = [
  'plugin.c',
  'gstvideoparserselement.c',
  'h263parse.c',
  'gsth263parse.c',
  'gstdiracparse.c',
  'dirac_parse.c',
  'gsth264parse.c',
  'gstmpegvideoparse.c',
  'gstmpeg4videoparse.c',
  'gstpngparse.c',
  'gstvc1parse.c',
  'gsth265parse.c',
  'gstvideoparseutils.c',
  'gstjpeg2000parse.c',
  'gstvp9parse.c',
  'gstav1parse.c',
]

gstvideoparsersbad = library('gstvideoparsersbad',
  vparse_sources,
  c_args : gst_plugins_bad_args + [ '-DGST_USE_UNSTABLE_API' ],
  include_directories : [configinc],
  dependencies : [gstcodecparsers_dep, gstbase_dep, gstpbutils_dep, gstvideo_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstvideoparsersbad]
