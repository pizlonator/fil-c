mpeg2enc_opt = get_option('mpeg2enc').require(gpl_allowed, error_message: '''
  Plugin mpeg2enc explicitly required via options but GPL-licensed plugins disabled via options.
  Pass option -Dgpl=enabled to Meson to allow GPL-licensed plugins to be built.
  ''')

# mjpegtools upstream breaks API constantly and doesn't export the version in
# a header anywhere. The configure file has a lot of logic to support old
# versions, but it all seems untested and broken. Require 2.0.0. Can be changed
# if someone complains.
mjpegtools_dep = dependency('mjpegtools', version : '>=2.0.0', required : mpeg2enc_opt)
mpeg2enc_dep = cxx.find_library('mpeg2encpp', required : mpeg2enc_opt)

if mjpegtools_dep.found() and mpeg2enc_dep.found()
  no_warn_args = []
  foreach arg : [
      '-Wno-mismatched-tags', # mjpeg headers get class/struct mixed up
      '-Wno-header-guard',
    ]
    if cxx.has_argument(arg)
      no_warn_args += [arg]
    endif
  endforeach

  gstmpeg2enc = library('gstmpeg2enc',
    'gstmpeg2enc.cc',
    'gstmpeg2encoptions.cc',
    'gstmpeg2encoder.cc',
    'gstmpeg2encstreamwriter.cc',
    'gstmpeg2encpicturereader.cc',
    cpp_args : gst_plugins_bad_args + ['-DGST_MJPEGTOOLS_API=20000'] + no_warn_args,
    link_args : noseh_link_args,
    include_directories : [configinc, libsinc],
    dependencies : [gstvideo_dep, mjpegtools_dep, mpeg2enc_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstmpeg2enc]
endif
