rtmp_sources = [
  'gstrtmp.c',
  'gstrtmpelement.c',
  'gstrtmpsink.c',
  'gstrtmpsrc.c',
]

rtmp_dep = dependency('librtmp', required : get_option('rtmp'))

if rtmp_dep.found()
  gstrtmp = library('gstrtmp',
    rtmp_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc, libsinc],
    dependencies : [gstbase_dep, rtmp_dep] + winsock2,
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstrtmp]
endif
