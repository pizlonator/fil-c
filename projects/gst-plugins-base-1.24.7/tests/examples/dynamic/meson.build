dynamic_examples = ['addstream', 'codec-select', 'sprinkle', 'sprinkle2', 'sprinkle3']

foreach example : dynamic_examples
  executable(example, '@0@.c'.format(example),
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [gst_dep],
  install: false)
endforeach
