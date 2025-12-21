app_examples = [
  'appsrc_ex',
  'appsrc-stream',
  'appsrc-stream2',
  'appsrc-ra',
  'appsrc-seekable',
  'appsink-src',
  'appsink-src2',
]

foreach app : app_examples
  executable(app, '@0@.c'.format(app),
    c_args : gst_plugins_base_args,
    include_directories: [configinc, libsinc],
    dependencies : [gst_dep, app_dep],
    install: false)
endforeach
