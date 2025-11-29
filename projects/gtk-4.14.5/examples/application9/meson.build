app9_resources = gnome.compile_resources('exampleapp9_resources',
  'exampleapp.gresource.xml',
  source_dir: meson.current_source_dir())

app9_schemas = gnome.compile_schemas()

executable('exampleapp9',
  'main.c',
  'exampleapp.c',
  'exampleappwin.c',
  'exampleappprefs.c',
  app9_resources,
  app9_schemas,
  dependencies: libgtk_dep,
  c_args: common_cflags)
