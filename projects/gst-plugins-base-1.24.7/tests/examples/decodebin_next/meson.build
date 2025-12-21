foreach example : ['decodebin3', 'playbin-test', 'uridecodebin3-select-all']
  executable(example, '@0@.c'.format(example),
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [gst_dep],
  install: false)
endforeach
