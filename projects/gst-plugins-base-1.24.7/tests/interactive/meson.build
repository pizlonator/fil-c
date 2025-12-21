base_itests = [
  [ 'benchmark-appsink.c', false, [gst_base_dep, app_dep], true ],
  [ 'benchmark-appsrc.c', false, [gst_base_dep, app_dep], true ],
  [ 'benchmark-video-conversion.c', false, [gst_base_dep, video_dep], true ],
  [ 'audio-trickplay.c', false, [gst_controller_dep] ],
  [ 'playbin-text.c' ],
  [ 'stress-playbin.c' ],
  [ 'test-effect-switch.c' ],
  [ 'test-overlay-blending.c', false, [video_dep, gio_dep] ],
  [ 'test-scale.c' ],
  [ 'test-resample.c' ],
  [ 'test-box.c' ],
  [ 'test-reverseplay.c' ],
  [ 'stress-videooverlay.c', not x11_dep.found(), [x11_dep, video_dep] ],
  [ 'output-selector-test.c', not x11_dep.found(), [x11_dep] ],
  [ 'input-selector-test.c', not x11_dep.found(), [x11_dep] ],
  [ 'test-colorkey.c', not x11_dep.found() or not gtk_x11_dep.found(), [x11_dep, gtk_x11_dep, video_dep]],
  [ 'test-videooverlay.c', not x11_dep.found() or not gtk_x11_dep.found(), [x11_dep, gtk_x11_dep, video_dep]],
  [ 'test-textoverlay.c', not pango_dep.found(), [pango_dep] ],
  [ 'playback/test.c' ],
  [ 'playback/decodetest.c' ],
  [ 'playback/test2.c' ],
  [ 'playback/test3.c' ],
  [ 'playback/test4.c' ],
  [ 'playback/test5.c' ],
  [ 'playback/test6.c' ],
  [ 'playback/test7.c' ],
]

itest_deps = [gst_dep]

foreach itest : base_itests
  fname = itest.get(0)
  itest_name = fname.split('.').get(0).underscorify()
  skip_itest = false
  is_bench = false
  extra_deps = [ ]

  if itest.length() >= 4
    is_bench = itest.get(3)
  endif

  if itest.length() >= 3
    extra_deps = itest.get(2)
  endif

  if itest.length() >= 2
    skip_itest = itest.get(1)
  endif

  if not skip_itest
    exe = executable(itest_name, fname,
      include_directories : [configinc],
      c_args : ['-DHAVE_CONFIG_H=1' ],
      dependencies : itest_deps + extra_deps,
    )
    if is_bench
      benchmark('bench_' + itest_name, exe)
    endif
  endif
endforeach
