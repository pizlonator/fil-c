alsa_sources = [
  'gstalsa.c',
  'gstalsadeviceprovider.c',
  'gstalsamidisrc.c',
  'gstalsaelement.c',
  'gstalsaplugin.c',
  'gstalsasink.c',
  'gstalsasrc.c',
]

alsa_dep = dependency('alsa', version : '>=0.9.1', required : get_option('alsa'))

if alsa_dep.found()
  gstalsa = library('gstalsa',
    alsa_sources,
    c_args : gst_plugins_base_args,
    include_directories: [configinc, libsinc],
    dependencies : [alsa_dep, audio_dep, tag_dep, gst_dep, gst_base_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstalsa]
endif
