app8_resources = gnome.compile_resources('exampleapp8 resources',
  'exampleapp.gresource.xml',
  source_dir: meson.current_source_dir())

app8_schemas = gnome.compile_schemas()

executable('exampleapp8',
  'main.c',
  'exampleapp.c',
  'exampleappwin.c',
  'exampleappprefs.c',
  app8_resources,
  app8_schemas,
  dependencies: libgtk_dep,
  c_args: common_cflags)
