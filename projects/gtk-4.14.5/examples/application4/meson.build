app4_resources = gnome.compile_resources('exampleapp4_resources',
  'exampleapp.gresource.xml',
  source_dir: meson.current_source_dir())

executable('exampleapp4',
  'exampleapp.c', 'exampleappwin.c', 'main.c', app4_resources,
  dependencies: libgtk_dep,
  c_args: common_cflags)
