opus_sources = [
  'gstopus.c',
  'gstopusheader.c',
  'gstopusparse.c',
]

opus_dep = dependency('opus', version : '>= 0.9.4', required : get_option('opus'))

if opus_dep.found()
  gstopus = library('gstopusparse',
    opus_sources,
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstrtp_dep, gstpbutils_dep, gstaudio_dep,
                    gsttag_dep, opus_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstopus]
endif
