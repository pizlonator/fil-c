app5_resources = gnome.compile_resources('exampleapp5_resources',
  'exampleapp.gresource.xml',
  source_dir: meson.current_source_dir())

app5_schemas = gnome.compile_schemas()

executable('exampleapp5',
  'exampleapp.c', 'exampleappwin.c', 'main.c', app5_resources, app5_schemas,
  dependencies: libgtk_dep,
  c_args: common_cflags)
