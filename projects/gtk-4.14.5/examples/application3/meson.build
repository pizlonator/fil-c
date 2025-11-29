app3_resources = gnome.compile_resources('exampleapp3_resources',
  'exampleapp.gresource.xml',
  source_dir: meson.current_source_dir())

executable('exampleapp3',
  'exampleapp.c', 'exampleappwin.c', 'main.c', app3_resources,
  dependencies: libgtk_dep,
  c_args: common_cflags)
