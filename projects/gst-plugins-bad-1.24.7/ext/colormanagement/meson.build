lcms2_dep = dependency('lcms2', version : '>= 2.7', required : get_option('colormanagement'))
if lcms2_dep.found()
  gstcolormanagement = library('gstcolormanagement',
    'gstcolormanagement.c', 'gstlcms.c',
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, lcms2_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstcolormanagement]
endif
