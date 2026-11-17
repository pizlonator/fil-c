desktop = 'gnome-about-panel.desktop'
i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

print_renderer_deps = [
  dependency('gtk4'),
  epoxy_dep
]

executable(
  'test-info-cleanup',
  [ 'info-cleanup.c', 'test-info-cleanup.c' ],
  include_directories: [ top_inc ],
  dependencies: [ print_renderer_deps ],
  c_args: cflags,
)

executable(
  'gnome-control-center-print-renderer',
  'gnome-control-center-print-renderer.c',
  include_directories: [ top_inc ],
  dependencies: [ print_renderer_deps ],
  c_args: cflags,
  install: true,
  install_dir: control_center_libexecdir,
)

install_data(
  [ 'gnome-logo-text.svg', 'gnome-logo-text-dark.svg' ],
  install_dir: join_paths(control_center_datadir, 'icons')
)

subdir('icons')
