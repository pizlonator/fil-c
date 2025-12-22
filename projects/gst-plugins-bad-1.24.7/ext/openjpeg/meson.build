openjpeg_sources = [
  'gstopenjpeg.c',
  'gstopenjpegdec.c',
  'gstopenjpegenc.c',
]

openjpeg_cargs = []

openjpeg_dep = dependency('libopenjp2', version : '>=2.2',
                          fallback : ['libopenjp2', 'libopenjp2_dep'],
                          required : get_option('openjpeg'),
                          default_options: ['build_codec_apps=false'])

if openjpeg_dep.found()
  gstopenjpeg = library('gstopenjpeg',
    openjpeg_sources,
    c_args : gst_plugins_bad_args + openjpeg_cargs,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gst_dep, gstvideo_dep, openjpeg_dep,
		    gstcodecparsers_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstopenjpeg]
endif
