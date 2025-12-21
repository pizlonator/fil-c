# autotools build also passes -D_GNU_SOURCE but not sure why it's needed
if gtk_dep.found()
  executable('audiomix', 'audiomix.c',
    c_args : gst_plugins_base_args,
    include_directories: [configinc, libsinc],
    dependencies : [gst_dep, audio_dep, gtk_dep],
    install: false)
  executable('volume', 'volume.c',
    c_args : gst_plugins_base_args,
    include_directories: [configinc, libsinc],
    dependencies : [gst_dep, libm, audio_dep, gtk_dep],
    install: false)
endif
