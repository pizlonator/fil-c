webrtc_sources = [
  'gstwebrtcdsp.cpp',
  'gstwebrtcechoprobe.cpp',
  'gstwebrtcdspplugin.cpp'
]

webrtc_dep = dependency('webrtc-audio-processing-1', version : ['>= 1.0'],
                        required : get_option('webrtcdsp'))

if webrtc_dep.found()
  gstwebrtcdsp = library('gstwebrtcdsp',
    webrtc_sources,
    cpp_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstaudio_dep, gstbadaudio_dep, webrtc_dep],
    install : true,
    install_dir : plugins_install_dir,
    override_options : ['cpp_std=c++17'],
  )
  plugins += [gstwebrtcdsp]
endif
