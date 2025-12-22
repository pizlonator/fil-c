v4l2codecs_sources = [
  'plugin.c',
  'gstv4l2codecallocator.c',
  'gstv4l2codecav1dec.c',
  'gstv4l2codecdevice.c',
  'gstv4l2codech264dec.c',
  'gstv4l2codech265dec.c',
  'gstv4l2codecmpeg2dec.c',
  'gstv4l2codecpool.c',
  'gstv4l2codecvp8dec.c',
  'gstv4l2codecvp9dec.c',
  'gstv4l2decoder.c',
  'gstv4l2format.c',
  'gstv4l2codecalphadecodebin.c',
]

libgudev_dep = dependency('gudev-1.0', required: get_option('v4l2codecs'), allow_fallback: true)

if get_option('v4l2codecs').disabled()
  have_v4l2 = false
  message('V4L2 CODECs plugin is disabled')
else
  # Should only be built on Linux, check for Linux kernel headers even though
  # we have our own copy.
  have_v4l2 = cc.has_header('linux/videodev2.h')
  if get_option('v4l2codecs').enabled() and not have_v4l2
    error('V4L2i CODECs is requested but kernel headers were not found')
  endif

  # Find makedev in various header files. Different operating systems put the
  # macro in different header files.
  foreach name: ['mkdev', 'sysmacros', 'types']
    have_makedev = cc.has_header_symbol('sys/@0@.h'.format(name), 'makedev')
    cdata.set10('HAVE_MAKEDEV_IN_' + name.to_upper(), have_makedev)
  endforeach
endif


if have_v4l2 and libgudev_dep.found()
  gstv4l2codecs = library('gstv4l2codecs',
    v4l2codecs_sources,
    c_args : gst_plugins_bad_args,
    cpp_args: gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstcodecs_dep, gstallocators_dep, libgudev_dep,
                   gstpbutils_dep,],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstv4l2codecs]
endif
