androidmedia_sources = [
  'gstamcaudiodec.c',
  'gstamc.c',
  'gstamc-codec.c',
  'gstamc-format.c',
  'gstamcsurfacetexture.c',
  'gstamcvideodec.c',
  'gstamcvideoenc.c',
]

androidmedia_java_sources = [
  'org/freedesktop/gstreamer/androidmedia/GstAhcCallback.java',
  'org/freedesktop/gstreamer/androidmedia/GstAhsCallback.java',
  'org/freedesktop/gstreamer/androidmedia/GstAmcOnFrameAvailableListener.java',
]

amc_opt = get_option('androidmedia')
mgl_opt = get_option('magicleap')

if host_system != 'android' or (amc_opt.disabled() and mgl_opt.disabled())
  subdir_done()
endif

if not gstgl_dep.found()
  if amc_opt.enabled() or mgl_opt.enabled()
    error('androidmedia plugin enabled but GL support was not detected')
  endif
  subdir_done()
endif

# Check if we have MLSDK
ml_deps = []
have_mlsdk = true
foreach lib : ['ml_mediacodec', 'ml_mediacodeclist', 'ml_mediaformat']
  dep = cc.find_library(lib, required : mgl_opt)
  have_mlsdk = have_mlsdk and dep.found()
  ml_deps += dep
endforeach

extra_deps = []
extra_cargs = []
if have_mlsdk
  androidmedia_sources += [
    'magicleap/gstamc-ml.c',
    'magicleap/gstamc-codec-ml.c',
    'magicleap/gstamc-codeclist-ml.c',
    'magicleap/gstamc-format-ml.c',
    'magicleap/gstamc-surfacetexture-ml.c',
  ]
  extra_deps = ml_deps
  have_jni_h = false
  extra_cargs += [
    '-DPLUGIN_NAME=mlmedia',
    '-DPLUGIN_DESCRIPTION="Magic Leap Media plugin"'
  ]
  plugin_name = 'gstmlmedia'
else
  have_jni_h = cc.has_header('jni.h', required : amc_opt)
  extra_cargs += [
    '-DHAVE_JNI_H',
    '-DPLUGIN_NAME=androidmedia',
    '-DPLUGIN_DESCRIPTION="Android Media plugin"'
  ]
  androidmedia_sources += [
    'gstahcsrc.c',
    'gstahssrc.c',
    'gst-android-graphics-imageformat.c',
    'gst-android-hardware-camera.c',
    'gst-android-hardware-sensor.c',
    'gstjniutils.c',
    'jni/gstamc-jni.c',
    'jni/gstamc-codec-jni.c',
    'jni/gstamc-codeclist-jni.c',
    'jni/gstamc-format-jni.c',
    'jni/gstamcsurface.c',
    'jni/gstamcsurfacetexture-jni.c',
  ]

  # Build an Ndk support if its headers exists. We don't actually link into it
  # but use only type definations (and then rely on dlopen and dlsym), so this
  # doesn't affect compatibility with older Android versions.
  if cc.check_header('media/NdkMediaCodec.h')
    androidmedia_sources += [
      'ndk/gstamc-codec-ndk.c',
      'ndk/gstamc-format-ndk.c',
    ]
    extra_cargs += [ '-DHAVE_NDKMEDIA' ]
    extra_deps += [ cc.find_library('android', required : true) ]
  endif

  plugin_name = 'gstandroidmedia'
endif

if have_jni_h or have_mlsdk
  gstandroidmedia = library(plugin_name,
    androidmedia_sources,
    c_args : [gst_plugins_bad_args, extra_cargs],
    include_directories : [configinc],
    dependencies : [gstgl_dep, gstpbutils_dep, gstaudio_dep, gstvideo_dep,
                    gstphotography_dep, gmodule_dep, orc_dep, extra_deps],
    install : true,
    install_dir : plugins_install_dir)
  plugins += [gstandroidmedia]

  install_data(androidmedia_java_sources,
    install_dir : join_paths(get_option('datadir'), 'gst-android/ndk-build/androidmedia')
  )
endif
