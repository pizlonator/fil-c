opus_sources = [
  'gstopus.c',
  'gstopuselement.c',
  'gstopuscommon.c',
  'gstopusdec.c',
  'gstopusenc.c',
  'gstopusheader.c',
]

opus_dep = dependency('opus', version: '>= 0.9.4', required: get_option('opus'),
                      fallback: ['opus', 'opus_dep'], default_options: ['docs=disabled'])

if opus_dep.found()
  gstopus = library('gstopus',
    opus_sources,
    c_args : gst_plugins_base_args,
    link_args : noseh_link_args,
    include_directories: [configinc, libsinc],
    dependencies : [pbutils_dep, tag_dep, audio_dep, gst_dep, gst_base_dep, opus_dep, libm],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstopus]
endif
