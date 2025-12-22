microdns_dep = dependency('microdns', required: get_option('microdns'),
  fallback: ['libmicrodns', 'mdns_dep'])

if microdns_dep.found()
  incdirs = [configinc]

  gstmicrodns = library('gstmicrodns',
    ['gstmicrodns.c', 'gstmicrodnsdevice.c'],
    c_args : gst_plugins_bad_args,
    include_directories : incdirs,
    dependencies : [gst_dep, microdns_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstmicrodns]
endif
