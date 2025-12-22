examples = ['wpe']

foreach example : examples
  exe_name = example
  src_file = '@0@.c'.format(example)

  executable(exe_name,
    src_file,
    install: false,
    include_directories : [configinc],
    dependencies : [gst_dep],
    c_args : ['-DHAVE_CONFIG_H=1'],
  )
endforeach
