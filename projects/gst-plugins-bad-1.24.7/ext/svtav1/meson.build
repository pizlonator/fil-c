svtav1_sources = [
  'gstsvtav1enc.c',
]

svtav1_dep = dependency('SvtAv1Enc', version : '>= 1.1', required: get_option('svtav1'))

if svtav1_dep.found()
  gstsvtav1 = library('gstsvtav1',
    svtav1_sources,
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, svtav1_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstsvtav1]
endif
