directfb_dep = dependency('directfb', version : '>= 0.9.24', required : get_option('directfb'))

if directfb_dep.found()
  gstdirectfb = library('gstdirectfb',
    'dfbvideosink.c',
    c_args : gst_plugins_bad_args + cc.get_supported_arguments('-Wno-redundant-decls'),
    include_directories : [configinc],
    dependencies : [gstvideo_dep, directfb_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstdirectfb]
endif
