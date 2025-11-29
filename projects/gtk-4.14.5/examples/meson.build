examples = [
  'builder',
  'drawing',
  'grid-packing',
  'plugman',
  'search-bar',
  'sunny',
  'window-default',
]

foreach ex : examples
  # TODO: name clash with testsuite/gtk/builder, rename one of the two
  example_name = ex == 'builder' ? 'builder_example' : ex
  executable(example_name, '@0@.c'.format(ex),
             c_args: [
               '-DGTK_SRCDIR="@0@"'.format(meson.current_source_dir()),
             ] + common_cflags,
             dependencies: libgtk_dep)
endforeach

subdir('bp')
subdir('application1')
subdir('application2')
subdir('application3')
subdir('application4')
subdir('application5')
subdir('application6')
subdir('application7')
subdir('application8')
subdir('application9')
