if gtk_x11_dep.found()
  if cc.compiles('''
    #include <linux/input.h>
    #include <linux/joystick.h>
    void foo (void) {
      struct js_event js;
      js.type = JS_EVENT_AXIS;
    }''', name : 'Linux joystick headers needed by jseek example')
    executable('jsseek', 'jsseek.c',
      c_args : gst_plugins_base_args,
      include_directories: [configinc, libsinc],
      dependencies : [gst_dep, video_dep, libm, gtk_dep],
      install: false)
  endif
  executable('scrubby', 'scrubby.c',
    c_args : gst_plugins_base_args,
    include_directories: [configinc, libsinc],
    dependencies : [gst_dep, video_dep, libm, gtk_dep],
    install: false)
endif

executable('stepping', 'stepping.c',
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [libm, gst_dep, video_dep],
  install: false)

executable('stepping2', 'stepping2.c',
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [libm, gst_dep, video_dep],
  install: false)

executable('instant-rate-change', 'instant-rate-change.c',
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [libm, gst_dep, video_dep],
  install: false)

