app6_resources = gnome.compile_resources('exampleapp6_resources',
  'exampleapp.gresource.xml',
  source_dir: meson.current_source_dir())

app6_schemas = gnome.compile_schemas()

executable('exampleapp6',
  'exampleapp.c',
  'exampleappwin.c',
  'main.c',
  'exampleappprefs.c',
  app6_resources,
  app6_schemas,
  dependencies: libgtk_dep,
  c_args: common_cflags)
