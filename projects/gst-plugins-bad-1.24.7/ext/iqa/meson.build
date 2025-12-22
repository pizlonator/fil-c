iqa_opt = get_option('iqa').require(gpl_allowed, error_message: '''
  Plugin iqa explicitly required via options but (A)GPL-licensed plugins disabled via options.
  Pass option -Dgpl=enabled to Meson to allow (A)GPL-licensed plugins to be built.
  ''')

dssim_dep = dependency('dssim', required: iqa_opt,
    fallback: ['dssim', 'dssim_dep'])

if dssim_dep.found()
  gstiqa = library('gstiqa',
    'iqa.c',
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DHAVE_DSSIM'],
    include_directories : [configinc],
    dependencies : [gstvideo_dep, gstbase_dep, gst_dep, dssim_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstiqa]
endif

