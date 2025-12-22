if get_option('opencv').disabled()
  gstopencv_dep = disabler()
  subdir_done()
endif

opencv_sources = [
  'gstopencvutils.cpp',
  'gstopencvvideofilter.cpp',
]

opencv_headers = [
  'opencv-prelude.h',
  'gstopencvutils.h',
  'gstopencvvideofilter.h',
]

libopencv_headers = [
  'opencv2/bgsegm.hpp',
  'opencv2/calib3d.hpp',
  'opencv2/core.hpp',
  'opencv2/imgproc.hpp',
  'opencv2/objdetect.hpp',
  'opencv2/opencv.hpp',
  'opencv2/video.hpp',
  'opencv2/tracking.hpp',
]

gstopencv_cargs = ['-DGST_HAAR_CASCADES_DIR="@0@"']

opencv_dep = dependency('opencv', version : ['>= 3.0.0', '< 3.5.0'], required : false)
opencv_found = opencv_dep.found()
if not opencv_found
  opencv_dep = dependency('opencv4', version : ['>= 4.0.0'], required : false)
  opencv_found = opencv_dep.found()
endif

if opencv_found
  foreach h : libopencv_headers
    if not cxx.has_header(h, dependencies: opencv_dep)
      message('Needed header "' + h + '" not found')
      opencv_found = false
    endif
  endforeach
endif

if opencv_found
  opencv_prefix = opencv_dep.get_variable('prefix')
  gstopencv_cargs += ['-DOPENCV_PREFIX="' + opencv_prefix + '"']
  # opencv4 seems to ship with .pc file that references non-existent include dir
  # (/usr/include/opencv4/opencv instead of /usr/include/opencv4/opencv2)
  # clang 10 complains about the following header in opencv4
  # /usr/include/opencv4/opencv2/flann/logger.h:83:36: error: format string is not a string literal [-Werror,-Wformat-nonliteral]
  gstopencv_cargs += cxx.get_supported_arguments(['-Wno-missing-include-dirs', '-Wno-format-nonliteral'])

  # Check the data dir used by opencv for its xml data files
  # Use prefix from pkg-config to be compatible with cross-compilation
  r = run_command('test', '-d', opencv_prefix + '/share/opencv', check: false)
  if r.returncode() == 0
    gstopencv_cargs += '-DOPENCV_PATH_NAME="opencv"'
  else
    r = run_command('test', '-d', opencv_prefix + '/share/OpenCV', check: false)
    if r.returncode() == 0
      gstopencv_cargs += '-DOPENCV_PATH_NAME="OpenCV"'
    else
      r = run_command('test', '-d', opencv_prefix + '/share/opencv4', check: false)
      if r.returncode() == 0
        gstopencv_cargs += '-DOPENCV_PATH_NAME="opencv4"'
      else
        message('Unable to detect OpenCV data directory')
        opencv_found = false
      endif
    endif
  endif
endif

if opencv_found
  pkg_name = 'gstreamer-opencv-1.0'
  gstopencv = library('gstopencv-' + api_version,
    opencv_sources,
    c_args : gst_plugins_bad_args + ['-DBUILDING_GST_OPENCV'],
    cpp_args : gst_plugins_bad_args + gstopencv_cargs + ['-DBUILDING_GST_OPENCV', '-DG_LOG_DOMAIN="GStreamer-OpenCV"'],
    override_options : ['cpp_std=c++11'],
    include_directories : [configinc, libsinc],
    version : libversion,
    soversion : soversion,
    darwin_versions : osxversion,
    install : true,
    dependencies : [gstbase_dep, gstvideo_dep, opencv_dep],
  )

  gst_libraries += [[pkg_name, {'lib': gstopencv}]]
  gstopencv_dep = declare_dependency(link_with: gstopencv,
    include_directories : [libsinc],
    dependencies : [gstvideo_dep, opencv_dep])
  meson.override_dependency(pkg_name, gstopencv_dep)

  install_headers(opencv_headers, subdir : 'gstreamer-1.0/gst/opencv')
elif get_option('opencv').enabled()
  error('OpenCV support enabled but required dependencies were not found.')
else
  gstopencv_dep = disabler()
endif
