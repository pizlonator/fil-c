x265_opt = get_option('x265').require(gpl_allowed, error_message: '''
  Plugin x265 explicitly required via options but GPL-licensed plugins disabled via options.
  Pass option -Dgpl=enabled to Meson to allow GPL-licensed plugins to be built.
  ''')

x265_dep = dependency('x265', required: x265_opt)
if x265_dep.found()
  gstx265 = library('gstx265',
    'gstx265enc.c',
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstpbutils_dep, gstvideo_dep, x265_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstx265]
endif
