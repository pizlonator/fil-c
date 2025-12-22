have_registry = true # FIXME get_option('registry')

libparser = static_library('parser',
  'elements/parser.c',
  install : false,
  dependencies : [gst_dep, gstcheck_dep],
)

libparser_dep = declare_dependency(link_with: libparser,
  sources: ['elements/parser.h'])

# FIXME: automagic
exif_dep = dependency('libexif', version : '>= 0.6.16', required : false)

# Since nalutils API is internal, need to build it again
nalutils_dep = gstcodecparsers_dep.partial_dependency (compile_args: true, includes: true)

enable_gst_play_tests = get_option('gst_play_tests')
libsoup_dep = dependency('libsoup-2.4', version : '>=2.48', required : enable_gst_play_tests,
  fallback : ['libsoup', 'libsoup_dep'])

# name, condition when to skip the test and extra dependencies
base_tests = [
  [['elements/aesenc.c'], not aes_dep.found(), [aes_dep]],
  [['elements/aesdec.c'], not aes_dep.found(), [aes_dep]],
  [['elements/aiffparse.c'], get_option('aiff').disabled()],
  [['elements/asfmux.c'], get_option('asfmux').disabled()],
  [['elements/audiovisualizer.c'], get_option('audiovisualizers').disabled()],
  [['elements/autoconvert.c'], get_option('autoconvert').disabled()],
  [['elements/autovideoconvert.c'], get_option('autoconvert').disabled()],
  [['elements/avwait.c'], get_option('timecode').disabled()],
  [['elements/camerabin.c'], get_option('camerabin2').disabled()],
  [['elements/ccconverter.c'], not closedcaption_dep.found(), [gstvideo_dep]],
  [['elements/cccombiner.c'], not closedcaption_dep.found(), ],
  [['elements/ccextractor.c'], not closedcaption_dep.found(), ],
  [['elements/cudaconvert.c'], false, [gstgl_dep, gmodule_dep]],
  [['elements/cudafilter.c'], false, [gstgl_dep, gmodule_dep]],
  [['elements/d3d11colorconvert.c'], host_machine.system() != 'windows', ],
  [['elements/d3d11videosink.c'], host_machine.system() != 'windows', ],
  [['elements/fdkaac.c'], not fdkaac_dep.found(), ],
  [['elements/gdpdepay.c'], get_option('gdp').disabled()],
  [['elements/gdppay.c'], get_option('gdp').disabled()],
  [['elements/h263parse.c'], false, [libparser_dep, gstcodecparsers_dep]],
  [['elements/h264parse.c'], false, [libparser_dep, gstcodecparsers_dep]],
  [['elements/h264timestamper.c'], false, [libparser_dep, gstcodecparsers_dep]],
  [['elements/h265parse.c'], false, [libparser_dep, gstcodecparsers_dep]],
  [['elements/hlsdemux_m3u8.c'], not hls_dep.found(), [hls_dep]],
  [['elements/id3mux.c'], get_option('id3tag').disabled()],
  [['elements/interlace.c'], get_option('interlace').disabled()],
  [['elements/jpeg2000parse.c'], false, [libparser_dep, gstcodecparsers_dep]],
  [['elements/line21.c'], not closedcaption_dep.found(), ],
  [['elements/mfvideosrc.c'], host_machine.system() != 'windows', ],
  [['elements/mpegtsdemux.c'], get_option('mpegtsdemux').disabled(), [gstmpegts_dep]],
  [['elements/mpegtsmux.c'], get_option('mpegtsmux').disabled(), [gstmpegts_dep]],
  [['elements/mpeg4videoparse.c'], false, [libparser_dep, gstcodecparsers_dep]],
  [['elements/mpegvideoparse.c'], false, [libparser_dep, gstcodecparsers_dep]],
  [['elements/msdkh264enc.c'], not have_msdk, [msdk_dep]],
  [['elements/mxfdemux.c'], get_option('mxf').disabled()],
  [['elements/mxfmux.c'], get_option('mxf').disabled()],
  [['elements/nvenc.c'], false, [gstgl_dep, gmodule_dep]],
  [['elements/nvdec.c'], not gstgl_dep.found(), [gstgl_dep, gmodule_dep]],
  [['elements/svthevcenc.c'], not svthevcenc_dep.found(), [svthevcenc_dep]],
   [['elements/openjpeg.c'], not openjpeg_dep.found(), [openjpeg_dep]],
  [['elements/pcapparse.c'], false, [libparser_dep]],
  [['elements/pnm.c'], get_option('pnm').disabled()],
  [['elements/proxysink.c'], get_option('proxy').disabled()],
  [['elements/ristrtpext.c']],
  [['elements/rtponvifparse.c'], get_option('onvif').disabled()],
  [['elements/rtponviftimestamp.c'], get_option('onvif').disabled()],
  [['elements/rtpsrc.c'], get_option('rtp').disabled()],
  [['elements/rtpsink.c'], get_option('rtp').disabled()],
  [['elements/sdpdemux.c'], get_option('sdp').disabled(), [gstsdp_dep]],
  [['elements/srt.c'], not srt_dep.found(), [srt_dep]],
  [['elements/srtp.c'], not srtp_dep.found(), [srtp_dep]],
  [['elements/switchbin.c'], get_option('switchbin').disabled()],
  [['elements/videoframe-audiolevel.c'], get_option('videoframe_audiolevel').disabled()],
  [['elements/viewfinderbin.c']],
  [['elements/voamrwbenc.c'], not voamrwbenc_dep.found(), [voamrwbenc_dep]],
  [['elements/vp9parse.c'], false, [gstcodecparsers_dep]],
  [['elements/av1parse.c'], false, [gstcodecparsers_dep]],
  [['elements/wasapi.c'], host_machine.system() != 'windows', ],
  [['elements/wasapi2.c'], host_machine.system() != 'windows', ],
  [['libs/h264parser.c'], false, [gstcodecparsers_dep]],
  [['libs/h265parser.c'], false, [gstcodecparsers_dep]],
  [['libs/insertbin.c'], false, [gstinsertbin_dep]],
  [['libs/isoff.c'], false, [gstisoff_dep]],
  [['libs/nalutils.c', '../../gst-libs/gst/codecparsers/nalutils.c'], false, [nalutils_dep]],
  [['libs/mpegts.c'], false, [gstmpegts_dep]],
  [['libs/mpegvideoparser.c'], false, [gstcodecparsers_dep]],
  [['libs/mse.c'], false, [gstmse_private_test_dep]],
  [['libs/planaraudioadapter.c'], false, [gstbadaudio_dep]],
  [['libs/play.c'], not enable_gst_play_tests, [gstplay_dep, libsoup_dep]],
  [['libs/vc1parser.c'], false, [gstcodecparsers_dep]],
  [['libs/vp8parser.c'], false, [gstcodecparsers_dep]],
  [['libs/vp9parser.c'], false, [gstcodecparsers_dep]],
  [['libs/av1parser.c'], false, [gstcodecparsers_dep]],
  [['libs/h264bitwriter.c'], false, [gstcodecparsers_dep]],
  [['libs/h265bitwriter.c'], false, [gstcodecparsers_dep]],
  [['libs/vkformat.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/av1bitwriter.c'], false, [gstcodecparsers_dep]],
  [['libs/vkmemory.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/analyticsmeta.c'], false, [gstanalytics_dep]],
  [['elements/vkcolorconvert.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/vkwindow.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/vkdevice.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['elements/vkdeviceprovider.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/vkcommandpool.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/vkimage.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/vkinstance.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/vkimagebufferpool.c'], not gstvulkan_dep.found(), [gstvulkan_dep]],
  [['libs/vkvideodecode.c'], not gstvulkan_dep.found() or vulkan_conf.get('GST_VULKAN_HAVE_VIDEO_EXTENSIONS') != 1, [gstvulkan_dep]],
  [['libs/d3d11device.cpp'], not gstd3d11_dep.found(), [gstd3d11_dep]],
  [['libs/d3d11memory.c'], not gstd3d11_dep.found(), [gstd3d11_dep]],
  [['libs/cudamemory.c'], not gstcuda_dep.found(), [gstcuda_dep, gstcuda_stub_dep]],
]

# Make sure our headers are C++ clean
if add_languages('cpp', native: false, required: false)
  base_tests += [
    [['libs/gstlibscpp.cc'], false ],
  ]
endif

# FIXME: unistd dependency, unstable or not tested yet on windows
if host_machine.system() != 'windows'
  base_tests += [
    [['elements/assrender.c'], not ass_dep.found(), [ass_dep]],
    [['elements/avtpaafpay.c'], not avtp_dep.found(), [avtp_dep]],
    [['elements/avtpaafdepay.c'], not avtp_dep.found(), [avtp_dep]],
    [['elements/avtpcrfbase.c'], not avtp_dep.found(), [avtp_dep]],
    [['elements/avtpcrfcheck.c'], not avtp_dep.found(), [avtp_dep], ['../../ext/avtp/gstavtpcrfutil.c']],
    [['elements/avtpcrfsync.c'], not avtp_dep.found(), [avtp_dep], ['../../ext/avtp/gstavtpcrfutil.c', '../../ext/avtp/gstavtpcrfbase.c']],
    [['elements/avtpcrfutil.c'], not avtp_dep.found(), [avtp_dep], ['../../ext/avtp/gstavtpcrfutil.c', '../../ext/avtp/gstavtpcrfbase.c']],
    [['elements/avtpcvfpay.c'], not avtp_dep.found(), [avtp_dep]],
    [['elements/avtpcvfdepay.c'], not avtp_dep.found(), [avtp_dep]],
    [['elements/avtpsink.c'], not avtp_dep.found(), [avtp_dep]],
    [['elements/avtpsrc.c'], not avtp_dep.found(), [avtp_dep]],
    [['elements/clockselect.c'], get_option('debugutils').disabled()],
    [['elements/curlhttpsink.c'], not curl_dep.found(), [curl_dep]],
    [['elements/curlhttpsrc.c'], not curl_dep.found(), [curl_dep, gio_dep]],
    [['elements/curlfilesink.c'],
        not curl_dep.found() or not cdata.has('HAVE_UNISTD_H'), [curl_dep]],
    [['elements/curlftpsink.c'], not curl_dep.found(), [curl_dep]],
    [['elements/curlsmtpsink.c'], not curl_dep.found(), [curl_dep]],
    [['elements/dash_mpd.c'], not xml2_dep.found(), [xml2_dep]],
    [['elements/dtls.c'], not libcrypto_dep.found() or not openssl_dep.found (), [libcrypto_dep]],
    [['elements/faac.c'],
        not faac_dep.found() or not cc.has_header_symbol('faac.h', 'faacEncOpen') or not cdata.has('HAVE_UNISTD_H'),
        [faac_dep]],
    [['elements/faad.c'],
        not faad_dep.found() or not have_faad_2_7 or not cdata.has('HAVE_UNISTD_H'),
        [faad_dep]],
    [['elements/jifmux.c'],
        not exif_dep.found() or not cdata.has('HAVE_UNISTD_H'), [exif_dep]],
    [['elements/jpegparse.c'], not cdata.has('HAVE_UNISTD_H')],
    [['elements/netsim.c']],
    [['elements/shm.c'], not shm_enabled, shm_deps],
    [['elements/unixfd.c'], not gio_unix_dep.found()],
    [['elements/voaacenc.c'],
        not voaac_dep.found() or not cdata.has('HAVE_UNISTD_H'), [voaac_dep]],
    [['elements/webrtcbin.c'], not libnice_dep.found(), [gstwebrtc_dep]],
    [['elements/x265enc.c'], not x265_dep.found(), [x265_dep]],
    [['elements/zbar.c'], not zbar_dep.found(), [zbar_dep]],
    [['elements/zxing.c'], not zxing_dep.found(), [zxing_dep]],
    [['elements/lc3.c'], not lc3_dep.found(), [lc3_dep]],
  ]
endif

# linux only tests
if host_machine.system() == 'linux'
  base_tests += [
    [['elements/vapostproc.c'], not gstva_dep.found(), [gstva_dep]],
    [['elements/vacompositor.c'], not gstva_dep.found(), [gstva_dep]],
  ]
endif

fsmod = import('fs')
test_defines = [
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
  '-DGST_CHECK_TEST_ENVIRONMENT_BEACON="GST_STATE_IGNORE_ELEMENTS"',
  '-DGST_TEST_FILES_PATH="' + fsmod.as_posix(meson.current_source_dir()) + '/../files"',
  '-DTEST_PATH="' + fsmod.as_posix(meson.current_build_dir()) + '/media"',
  '-DDASH_MPD_DATADIR=' + fsmod.as_posix(meson.current_source_dir()) + '/elements/dash_mpd_data',
  '-DGST_USE_UNSTABLE_API',
]

test_deps = [gst_dep, gstapp_dep, gstbase_dep, gstbadaudio_dep,
             gstbasecamerabin_dep, gstphotography_dep, gstwebrtc_dep,
             gstmpegts_dep, gstmse_dep, gstplay_dep, gstplayer_dep,
             gstanalytics_dep, gstinsertbin_dep, gst_transcoder_dep,
             gstpbutils_dep, gstcontroller_dep, gstaudio_dep,
             gstvideo_dep, gstrtp_dep, gsturidownloader_dep,
             gstcheck_dep, gio_dep, gsttag_dep]

pluginsdirs = [  ]

if gst_dep.type_name() == 'pkgconfig'
  pbase = dependency('gstreamer-plugins-base-' + api_version)

  pluginsdirs = [gst_dep.get_variable('pluginsdir'),
                 pbase.get_variable('pluginsdir')]
  gst_plugin_scanner_dir = gst_dep.get_variable('pluginscannerdir')
else
  gst_plugin_scanner_dir = subproject('gstreamer').get_variable('gst_scanner_dir')
endif
gst_plugin_scanner_path = join_paths(gst_plugin_scanner_dir, 'gst-plugin-scanner')

extra_args = []
# XXX: our MinGW 32bits toolchain complians when ComPtr is in use
if host_system == 'windows' and cc.get_id() != 'msvc'
  mingw_args = cc.get_supported_arguments([
    '-Wno-redundant-decls',
  ])

  extra_args += mingw_args
endif

foreach t : base_tests
  fnames = t.get(0)
  test_name = fnames[0].split('.').get(0).underscorify()
  skip_test = false
  extra_deps = [ ]

  if t.length() >= 3
    extra_deps = t.get(2)
  endif

  if t.length() >= 2
    skip_test = t.get(1)
  endif

  extra_sources = t.get(3, [ ])

  if not skip_test
    exe = executable(test_name, fnames, extra_sources,
      include_directories : [configinc],
      c_args : gst_plugins_bad_args + test_defines + extra_args,
      cpp_args : gst_plugins_bad_args + test_defines + extra_args,
      dependencies : [libm] + test_deps + extra_deps,
    )

    env = environment()
    env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
    env.set('CK_DEFAULT_TIMEOUT', '20')
    env.set('GST_STATE_IGNORE_ELEMENTS', '')
    env.set('GST_PLUGIN_PATH_1_0', [meson.global_build_root()] + pluginsdirs)
    env.set('GST_REGISTRY', join_paths(meson.current_build_dir(), '@0@.registry'.format(test_name)))
    env.set('GST_PLUGIN_LOADING_WHITELIST', 'gstreamer', 'gst-plugins-base',
      'gst-plugins-good', 'gst-plugins-ugly','gst-libav', 'libnice',
      'gst-plugins-bad@' + meson.project_build_root())
    env.set('GST_PLUGIN_SCANNER_1_0', gst_plugin_scanner_path)
    test(test_name, exe, env: env, timeout: 3 * 60)
  endif
endforeach

# orc tests
orc_tests = [
  ['orc_bayer', files('../../gst/bayer/gstbayerorc.orc')],
  ['orc_fieldanalysis', files('../../gst/fieldanalysis/gstfieldanalysisorc.orc')],
  ['orc_gaudieffects', files('../../gst/gaudieffects/gstgaudieffectsorc.orc')],
  ['orc_scenechange', files('../../gst/videofilters/gstscenechangeorc.orc')],
]

orc_test_dep = dependency('', required : false)
if have_orcc
  # FIXME: there should really just be a separate orc-test-0.4.pc file for this
  if orc_dep.type_name() == 'pkgconfig'
    orc_test_dep = cc.find_library('orc-test-0.4', required : false)
  else
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

if enable_gst_play_tests
  subdir ('media')
endif
