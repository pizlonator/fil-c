webp_sources = [
  'gstwebp.c',
  'gstwebpdec.c',
  'gstwebpenc.c',
]

webp_dep = dependency('libwebp', version : '>= 0.2.1', required : get_option('webp'))
webpmux_dep = dependency('libwebpmux', version : '>= 0.2.1', required : get_option('webp'))

if webp_dep.found() and webpmux_dep.found()
  gstwebp = library('gstwebp',
    webp_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, webp_dep, webpmux_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstwebp]
endif
