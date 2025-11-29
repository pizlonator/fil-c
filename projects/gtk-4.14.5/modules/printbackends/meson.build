print_backends = []

printbackends_subdir = 'gtk-4.0/@0@/printbackends'.format(gtk_binary_version)
printbackends_install_dir = join_paths(get_option('libdir'), printbackends_subdir)
gio_module_dirs += printbackends_install_dir

printbackends_args = [
  '-DGTK_COMPILATION',
  '-DGTK_DISABLE_DEPRECATION_WARNINGS',
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
] + common_cflags

# Use cpdb backend if present and enabled.
# If not, use cups if present.

if get_option('print-cpdb').enabled() and cpdb_dep.found()
  print_backends += 'cpdb'
  shared_module('printbackend-cpdb',
    sources: [
      'gtkprintbackendcpdb.c',
      'gtkprintercpdb.c',
      'gtkprintbackendutils.c',
    ],
    c_args: printbackends_args,
    dependencies: [libgtk_dep, cpdb_dep],
    install_dir: printbackends_install_dir,
    install: true,
  )
elif cups_dep.found()
  print_backends += 'cups'
  shared_module('printbackend-cups',
    sources: [
      'gtkprintbackendcups.c',
      'gtkprintercups.c',
      'gtkcupsutils.c',
      'gtkcupssecretsutils.c',
      'gtkprintbackendutils.c',
    ],
    c_args: printbackends_args,
    dependencies: [libgtk_dep, cups_dep, colord_dep],
    name_suffix: module_suffix,
    install_dir: printbackends_install_dir,
    install: true,
  )
endif

# The 'file' print backend cannot be disabled
print_backends += 'file'
shared_module('printbackend-file',
  sources: 'gtkprintbackendfile.c',
  c_args: printbackends_args,
  dependencies: libgtk_dep,
  name_suffix: module_suffix,
  install_dir: printbackends_install_dir,
  install: true,
)

cdata.set_quoted('GTK_PRINT_BACKENDS', ','.join(print_backends))
