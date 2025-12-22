uvch264_sources = [
  'gstuvch264.c',
  'gstuvch264_mjpgdemux.c',
  'gstuvch264_src.c',
  'gstuvch264deviceprovider.c',
  'uvc_h264.c',
]

libgudev_dep = dependency('gudev-1.0', required : get_option('uvch264'), allow_fallback: true)
libusb_dep = dependency('libusb-1.0', required : get_option('uvch264'))
has_uvcvideo_h = cc.has_header('linux/uvcvideo.h')
if not has_uvcvideo_h and get_option('uvch264').enabled()
  error('uvch264 plugin enabled but uvcvideo.h not found')
endif

if libgudev_dep.found() and libusb_dep.found() and has_uvcvideo_h
  gstuvch264 = library('gstuvch264',
    uvch264_sources,
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, gstallocators_dep,
                    gstbasecamerabin_dep, libgudev_dep, libusb_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstuvch264]
endif
