if not gstva_dep.found()
  subdir_done()
endif

gtk_dep = dependency('gtk+-3.0', required : get_option('examples'))
gtk_x11_dep = dependency('gtk+-x11-3.0', required : get_option('examples'))
x11_dep = dependency('x11', required : get_option('examples'))
libva_x11_dep = dependency('libva-x11', version: libva_req, required: get_option('examples'))

if gtk_dep.found() and gtk_x11_dep.found() and x11_dep.found() and libva_x11_dep.found()
  executable('va-x11-render',
    'main.c',
    install: false,
    include_directories : [configinc],
    dependencies : [gtk_dep, gtk_x11_dep, x11_dep, gst_dep, gstapp_dep, gstvideo_dep, gstva_dep, libva_x11_dep],
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  )
endif

executable('multiple-vpp',
  'multiple-vpp.c',
  install: false,
  include_directories : [configinc],
  dependencies : [gst_dep, gstvideo_dep, gstva_dep, gstcontroller_dep],
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
)

executable('vaenc-dynamic-reconfigure',
  ['vaenc-dynamic-reconfigure.c', '../key-handler.c'],
  include_directories : [configinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  install: false)
