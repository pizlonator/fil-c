ass_dep = dependency('libass', version : '>= 0.10.2', required : get_option('assrender'))
if ass_dep.found()
  gstassrender = library('gstassrender',
    'gstassrender.c',
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, ass_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstassrender]
endif
