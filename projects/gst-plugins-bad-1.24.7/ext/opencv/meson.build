if get_option('opencv').disabled()
  opencv_dep = disabler()
  subdir_done()
endif

gstopencv_sources = [
  'gstcvdilate.cpp',
  'gstcvdilateerode.cpp',
  'gstcvequalizehist.cpp',
  'gstcverode.cpp',
  'gstcvlaplace.cpp',
  'gstcvsmooth.cpp',
  'gstcvsobel.cpp',
  'gstdisparity.cpp',
  'gstedgedetect.cpp',
  'gstfaceblur.cpp',
  'gstfacedetect.cpp',
  'gstgrabcut.cpp',
  'gsthanddetect.cpp',
  'gstmotioncells.cpp',
  'gstopencv.cpp',
  'gstretinex.cpp',
  'gstsegmentation.cpp',
  'gstskindetect.cpp',
  'gsttemplatematch.cpp',
  'gsttextoverlay.cpp',
  'MotionCells.cpp',
  'motioncells_wrapper.cpp',
  'gstdewarp.cpp',
  'camerautils.cpp',
  'cameraevent.cpp',
  'gstcameracalibrate.cpp',
  'gstcameraundistort.cpp',
  'gstcvtracker.cpp'
]

if opencv_found
  gstopencv = library('gstopencv',
    gstopencv_sources,
    cpp_args : gst_plugins_bad_args + gstopencv_cargs + [ '-DGST_USE_UNSTABLE_API' ],
    link_args : [noseh_link_args, '-lopencv_tracking'],
    include_directories : [configinc, libsinc],
    dependencies : [gstbase_dep, gstvideo_dep, opencv_dep, gstopencv_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstopencv]
elif get_option('opencv').enabled()
  error('OpenCV support enabled but required dependencies were not found.')
endif
