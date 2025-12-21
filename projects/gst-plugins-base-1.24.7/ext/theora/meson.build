theora_sources = [
  'gsttheora.c',
  'gsttheoraparse.c',
]

theoradec_dep = dependency('theoradec', version : '>=1.1', required : get_option('theora'))
theoraenc_dep = dependency('theoraenc', version : '>=1.1', required : get_option('theora'))
theora_deps = []

if theoradec_dep.found() or theoraenc_dep.found()
  if theoradec_dep.found()
    theora_sources += [ 'gsttheoradec.c' ]
    theora_deps += theoradec_dep
  endif
  if theoraenc_dep.found()
    theora_sources += [ 'gsttheoraenc.c' ]
    theora_deps += theoraenc_dep
  endif
  gsttheora = library('gsttheora',
    theora_sources,
    c_args : gst_plugins_base_args,
    link_args : noseh_link_args,
    include_directories: [configinc, libsinc],
    dependencies : theora_deps + [video_dep, tag_dep, gst_dep, gst_base_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gsttheora]
endif
