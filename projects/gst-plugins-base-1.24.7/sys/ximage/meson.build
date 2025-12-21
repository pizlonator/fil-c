no_warn_args = []
# XKeycodeToKeysym is deprecated, but we use it when Xkb is unavailable
if cc.has_argument ('-Wno-deprecated-declarations')
  no_warn_args += '-Wno-deprecated-declarations'
endif

gstximage = library('gstximagesink',
  'ximagesink.c', 'ximage.c', 'ximagepool.c',
  c_args : gst_plugins_base_args + no_warn_args,
  include_directories: [configinc, libsinc],
  dependencies : [video_dep, gst_base_dep, gst_dep, x11_dep, xshm_dep, xi_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstximage]
