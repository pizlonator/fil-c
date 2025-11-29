app7_resources = gnome.compile_resources('exampleapp7_resources',
  'exampleapp.gresource.xml',
  source_dir: meson.current_source_dir())

app7_schemas = gnome.compile_schemas()

executable('exampleapp7',
  'main.c',
  'exampleapp.c',
  'exampleappwin.c',
  'exampleappprefs.c',
  app7_resources,
  app7_schemas,
  dependencies: libgtk_dep,
  c_args: common_cflags)
