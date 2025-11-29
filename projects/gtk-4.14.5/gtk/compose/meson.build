compose_parse = executable('compose-parse',
  sources: 'compose-parse.c',
  c_args: gtk_cargs + common_cflags,
  include_directories: [confinc, gtkinc],
  dependencies: libgtk_static_dep,
  install: false,
)
