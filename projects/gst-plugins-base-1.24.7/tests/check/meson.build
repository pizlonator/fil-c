have_registry = true # FIXME get_option('registry')

# name, condition when to skip the test and extra dependencies
base_tests = [
  [ 'gst/typefindfunctions.c', not have_registry ],
  [ 'libs/audio.c' ],
  [ 'libs/audiocdsrc.c' ],
  [ 'libs/audiodecoder.c' ],
  [ 'libs/audioencoder.c' ],
  [ 'libs/audiosink.c' ],
  [ 'libs/baseaudiovisualizer.c' ],
  [ 'libs/discoverer.c' ],
  [ 'libs/dsd.c' ],
  [ 'libs/fft.c' ],
  [ 'libs/libsabi.c', false, [ gstgl_dep, gstglx11_dep, gstglwayland_dep, gstglegl_dep ] ],
  [ 'libs/mikey.c' ],
  [ 'libs/navigation.c' ],
  [ 'libs/pbutils.c' ],
  [ 'libs/profile.c' ],
  [ 'libs/rtp.c' ],
  [ 'libs/rtpbasedepayload.c' ],
  [ 'libs/rtpbasepayload.c' ],
  [ 'libs/rtphdrext.c' ],
  [ 'libs/rtpmeta.c' ],
  [ 'libs/rtsp.c' ],
  [ 'libs/sdp.c' ],
  [ 'libs/tag.c' ],
  [ 'libs/video.c' ],
  [ 'libs/videoanc.c' ],
  [ 'libs/videoencoder.c' ],
  [ 'libs/videodecoder.c' ],
  [ 'libs/videotimecode.c' ],
  [ 'libs/xmpwriter.c' ],
  [ 'elements/adder.c', get_option('adder').disabled()],
  [ 'elements/appsink.c', get_option('app').disabled()],
  [ 'elements/appsrc.c', get_option('app').disabled()],
  [ 'elements/audioconvert.c', get_option('audioconvert').disabled()],
  [ 'elements/audiointerleave.c', get_option('audiomixer').disabled()],
  [ 'elements/audiomixer.c', get_option('audiomixer').disabled(), [ gst_controller_dep ] ],
  [ 'elements/audiorate.c', get_option('audiorate').disabled()],
  [ 'elements/audiotestsrc.c', get_option('audiotestsrc').disabled()],
  [ 'elements/audioresample.c', get_option('audioresample').disabled()],
  [ 'elements/compositor.c', get_option('compositor').disabled()],
  [ 'elements/decodebin.c', get_option('playback').disabled()],
  [ 'elements/overlaycomposition.c', get_option('overlaycomposition').disabled()],
  [ 'elements/playbin.c', get_option('playback').disabled()],
  [ 'elements/playsink.c', get_option('playback').disabled()],
  [ 'elements/streamsynchronizer.c', get_option('playback').disabled()],
  [ 'elements/subparse.c', get_option('subparse').disabled()],
  [ 'elements/urisourcebin.c', get_option('playback').disabled()],
  [ 'elements/videoconvert.c', get_option('videoconvertscale').disabled()],
  [ 'elements/videorate.c', get_option('videorate').disabled()],
  [ 'elements/videoscale.c', get_option('videoconvertscale').disabled()],
  [ 'elements/videotestsrc.c', get_option('videotestsrc').disabled()],
  [ 'elements/volume.c', get_option('volume').disabled(), [ gst_controller_dep ] ],
  [ 'generic/clock-selection.c' ],
  [ 'generic/states.c', false, [ gmodule_dep ] ],
  [ 'pipelines/simple-launch-lines.c' ],
  [ 'pipelines/basetime.c' ],
  [ 'pipelines/capsfilter-renegotiation.c' ],
  [ 'pipelines/gio.c' ],
  [ 'pipelines/streamsynchronizer.c' ],
]

# FIXME: unistd dependency, unstable or not tested yet on windows
if host_machine.system() != 'windows'
  base_tests += [
    [ 'libs/allocators.c', host_machine.system() != 'linux' ],
    [ 'libs/rtspconnection.c' ],
    [ 'elements/libvisual.c', not is_variable('libvisual_dep') or not libvisual_dep.found() ],
    [ 'elements/encodebin.c', not theoraenc_dep.found() or not vorbisenc_dep.found() ],
    [ 'elements/multifdsink.c', not core_conf.has('HAVE_SYS_SOCKET_H') or not core_conf.has('HAVE_UNISTD_H') ],
    # FIXME: multisocketsink test on windows/msvc
    [ 'elements/multisocketsink.c', not core_conf.has('HAVE_SYS_SOCKET_H') or not core_conf.has('HAVE_UNISTD_H') ],
    [ 'elements/playbin-complex.c', not ogg_dep.found() ],
    [ 'elements/textoverlay.c', not pango_dep.found() ],
    [ 'elements/theoradec.c', not ogg_dep.found() or not theoradec_dep.found() ],
    [ 'elements/vorbisdec.c', not vorbis_dep.found(), [ vorbis_dep, vorbisenc_dep ] ],
    [ 'elements/vorbistag.c', not vorbisenc_dep.found(), [ vorbis_dep, vorbisenc_dep ] ],
    [ 'pipelines/oggmux.c', not ogg_dep.found(), [ ogg_dep, ] ],
    # FIXME: tcp test on windows/msvc
    [ 'pipelines/tcp.c', not core_conf.has('HAVE_SYS_SOCKET_H') or not core_conf.has('HAVE_UNISTD_H'), [giounix_dep] ],
    [ 'pipelines/theoraenc.c', not theoraenc_dep.found(), [ theoraenc_dep ] ],
    [ 'pipelines/vorbisenc.c', not vorbisenc_dep.found() ],
    [ 'pipelines/vorbisdec.c', not vorbisenc_dep.found() ],
  ]
endif

# FIXME: Unstable on Windows
if build_gstgl and host_machine.system() != 'windows'
  base_tests += [
    [ 'libs/gstglapi.c', not build_gstgl, [gstgl_dep]],
    [ 'libs/gstglcolorconvert.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglcontext.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglfeature.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglformat.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglheaders.c', not build_gstgl, [gstgl_dep, gstglx11_dep, gstglwayland_dep, gstglegl_dep]],
    [ 'libs/gstglmatrix.c', not build_gstgl, [gstgl_dep]],
    [ 'libs/gstglmemory.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglquery.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglshader.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglsl.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglslstage.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglupload.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'libs/gstglvideomixerelement.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'elements/glimagesink.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'elements/glbin.c', not build_gstgl ],
    [ 'pipelines/gl-launch-lines.c', not build_gstgl ],
    [ 'elements/glfilter.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'elements/glstereo.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
    [ 'elements/glmixer.c', not build_gstgl, [gstgl_dep, gstglproto_dep]],
  ]
endif

# Make sure our headers are C++ clean
if have_cxx
  base_tests += [
    [ 'libs/gstlibscpp.cc', false, [  ] ],
  ]
endif

fsmod = import('fs')
test_defines = [
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
  '-DGST_CHECK_TEST_ENVIRONMENT_BEACON="GST_PLUGIN_LOADING_WHITELIST"',
  '-DGST_TEST_FILES_PATH="' + fsmod.as_posix(meson.current_source_dir()) + '/../files"',
  '-DGST_TEST_FILE="' + fsmod.as_posix(meson.current_source_dir()) + '/../files/partialframe.mjpeg"', # for libs/discoverer
  '-DGST_USE_UNSTABLE_API',
]

if build_gstgl
  test_defines += ['-DTEST_GST_GL_ABI_CHECK']
endif

test_deps = [gst_dep, gst_base_dep, gst_net_dep, gst_check_dep, audio_dep,
  video_dep, pbutils_dep, rtp_dep, rtsp_dep, tag_dep, allocators_dep, app_dep,
  fft_dep, riff_dep, sdp_dep, gio_dep, valgrind_dep]

foreach t : base_tests
  fname = t.get(0)
  test_name = fname.split('.').get(0).underscorify()
  skip_test = false
  extra_deps = [ ]

  if t.length() >= 3
    extra_deps = t.get(2)
  endif

  if t.length() >= 2
    skip_test = t.get(1)
  endif

  if not skip_test
    exe = executable(test_name, fname,
      include_directories : [configinc],
      c_args : ['-DHAVE_CONFIG_H=1' ] + test_defines,
      cpp_args : gst_plugins_base_args,
      dependencies : [libm] + test_deps + extra_deps,
    )

    env = environment()
    env.set('GST_PLUGIN_PATH_1_0', meson.global_build_root(), pluginsdirs)
    env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
    env.set('GST_STATE_IGNORE_ELEMENTS', 'cdio cdparanoiasrc libvisual_ alsasrc alsasink')
    env.set('CK_DEFAULT_TIMEOUT', '20')
    env.set('GST_TAG_LICENSE_TRANSLATIONS_DICT', join_paths(gst_tag_dir, 'license-translations.dict'))
    env.set('GST_PLUGIN_LOADING_WHITELIST', 'gstreamer',
        'gst-plugins-base@' + meson.project_build_root())
    env.set('GST_REGISTRY', join_paths(meson.current_build_dir(), '@0@.registry'.format(test_name)))
    env.set('GST_PLUGIN_SCANNER_1_0', gst_plugin_scanner_path)
    test(test_name, exe, env: env, timeout: 3 * 60)
  endif
endforeach

# videoscale tests (split in groups)
foreach group : [1, 2, 3, 4, 5, 6]
  vscale_test_name = 'elements-videoscale-@0@'.format(group)
  exe = executable(vscale_test_name, join_paths('elements', 'videoscale.c'),
      include_directories : [configinc],
      c_args : ['-DHAVE_CONFIG_H=1', '-DVSCALE_TEST_GROUP=@0@'.format(group) ] + test_defines,
      dependencies : [libm] + test_deps + extra_deps)

  # TODO Use env.copy when it is in meson
  env = environment()
  env.set('GST_PLUGIN_PATH_1_0', meson.global_build_root(), pluginsdirs)
  env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
  env.set('GST_STATE_IGNORE_ELEMENTS', 'cdio cdparanoiasrc libvisual_ alsasrc alsasink')
  env.set('CK_DEFAULT_TIMEOUT', '20')
  env.set('GST_TAG_LICENSE_TRANSLATIONS_DICT', join_paths(gst_tag_dir, 'license-translations.dict'))
  env.set('GST_PLUGIN_LOADING_WHITELIST', 'gstreamer',
      'gst-plugins-base@' + meson.project_build_root())
  env.set('GST_REGISTRY', join_paths(meson.current_build_dir(), '@0@.registry'.format(vscale_test_name)))
  env.set('GST_PLUGIN_SCANNER_1_0', gst_plugin_scanner_path)

  test(vscale_test_name, exe, env: env, timeout: 3 * 60)
endforeach

# orc tests
orc_tests = [
  ['orc_audio', files('../../gst-libs/gst/audio/gstaudiopack.orc')],
  ['orc_video', files('../../gst-libs/gst/video/video-orc.orc')],
  ['orc_adder', files('../../gst/adder/gstadderorc.orc')],
  ['orc_audiomixer', files('../../gst/audiomixer/gstaudiomixerorc.orc')],
  ['orc_compositor', files('../../gst/compositor/compositororc.orc')],
  ['orc_volume', files('../../gst/volume/gstvolumeorc.orc')],
  ['orc_videotestsrc', files('../../gst/videotestsrc/gstvideotestsrcorc.orc')],
]

orc_test_dep = dependency('', required : false)
if have_orcc
  # FIXME: there should really just be a separate orc-test-0.4.pc file for this
  if orc_dep.type_name() == 'pkgconfig'
    orc_test_dep = cc.find_library('orc-test-0.4', required : false)
  endif
  if not orc_test_dep.found()
    orc_test_dep = dependency('', fallback: ['orc', 'orc_test_dep'], required: false)
  endif
endif

if have_orcc and orc_test_dep.found()
  foreach t : orc_tests
    tname = t[0]
    torcfile = t[1]
    tcfilename = '@0@.c'.format(tname)
    test_c = custom_target(tcfilename,
      output: tcfilename,
      input: torcfile,
      command: [orcc, '--include', 'stdint.h', '--test', '-o', '@OUTPUT@', '@INPUT@'])
    test_exe = executable(tname, test_c, dependencies: [orc_dep, orc_test_dep])
    test(tname, test_exe)
  endforeach
endif
