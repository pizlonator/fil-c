gtk_dep = dependency('gtk+-3.0', required : get_option('examples'))
gdk_x11_dep = dependency('gdk-x11-3.0', required : get_option('examples'))
if gtk_dep.found() and gdk_x11_dep.found()
  fsmod = import('fs')
  camera2_args = [
    '-DGST_USE_UNSTABLE_API',
    '-DCAMERA_APPS_UIDIR="@0@"'.format(fsmod.as_posix(meson.current_source_dir())),
    cc.get_supported_link_arguments('-fvisibility=default'),
  ]
  gmodule_export_dep = dependency('gmodule-export-2.0')
  executable('gst-camera2', 'gst-camera2.c',
    include_directories : [configinc],
    dependencies : [gstphotography_dep, gtk_dep, gdk_x11_dep, gst_dep, gstvideo_dep, gstpbutils_dep, gmodule_export_dep],
    c_args : gst_plugins_bad_args + camera2_args,
    install: false)
endif

x11_dep = dependency('x11', required : get_option('examples'))
if x11_dep.found()
  executable('gst-camerabin2-test', 'gst-camerabin2-test.c',
    include_directories : [configinc],
    dependencies : [gstphotography_dep, x11_dep, gst_dep, gstvideo_dep, gstpbutils_dep],
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
    install: false)
endif
