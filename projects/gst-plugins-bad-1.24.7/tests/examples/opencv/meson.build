examples = ['gstmotioncells_dynamic_test', 'gsthanddetect_test', 'gstfacedetect_test']

foreach example : examples
  exe_name = example
  src_file = '@0@.c'.format(example)

  executable(exe_name,
    src_file,
    include_directories: [configinc],
    dependencies: [gst_dep, gstbase_dep],
    c_args : gst_plugins_bad_args,
    install: false)
endforeach
