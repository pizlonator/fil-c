rsvg_sources = [
  'gstrsvg.c',
  'gstrsvgdec.c',
  'gstrsvgoverlay.c',
]

rsvg_dep = dependency('librsvg-2.0', version : '>= 2.36.2', required : get_option('rsvg'))
if rsvg_dep.found()
  gstrsvg = library('gstrsvg',
    rsvg_sources,
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, rsvg_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstrsvg]
endif
