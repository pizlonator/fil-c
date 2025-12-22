uvcgadget_sources = [
  'gstuvcsink.c',
  'configfs.c',
  'uvc.c'
]

libgudev_dep = dependency('gudev-1.0', required: get_option('uvcgadget'))

if get_option('uvcgadget').disabled()
  have_v4l2 = false
  message('UVCSink plugin is disabled')
else
  # Should only be built on Linux, check for Linux kernel headers even though
  # we have our own copy.
  have_v4l2 = cc.has_header('linux/videodev2.h') or cc.has_header('sys/videodev2.h') or cc.has_header('sys/videoio.h')
  if get_option('uvcgadget').enabled() and not have_v4l2
    error('UVCGADGET is requested but kernel headers were not found')
  endif

  # Find makedev in various header files. Different operating systems put the
  # macro in different header files.
  foreach name: ['mkdev', 'sysmacros', 'types']
    have_makedev = cc.has_header_symbol('sys/@0@.h'.format(name), 'makedev')
    cdata.set10('HAVE_MAKEDEV_IN_' + name.to_upper(), have_makedev)
  endforeach
endif

if have_v4l2 and libgudev_dep.found()
  gstuvcgadget = library('gstuvcgadget',
    uvcgadget_sources,
    c_args : gst_plugins_bad_args,
    cpp_args: gst_plugins_bad_args,
    include_directories : [configinc, include_directories('../v4l2codecs')],
    dependencies : [gstbase_dep, gstvideo_dep, gstallocators_dep, libgudev_dep, gstpbutils_dep,],
    install : true,
    install_dir : plugins_install_dir,
  )
  pkgconfig.generate(gstuvcgadget, install_dir : plugins_pkgconfig_install_dir)
  plugins += [gstuvcgadget]
endif
