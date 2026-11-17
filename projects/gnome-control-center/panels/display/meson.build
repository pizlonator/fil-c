panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

sources = files(
  'cc-display-panel.c',
  'cc-display-arrangement.c',
  'cc-display-config.c',
  'cc-display-config-dbus.c',
  'cc-display-config-manager-dbus.c',
  'cc-display-config-manager.c',
  'cc-display-settings.c',
  'cc-night-light-page.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  source_dir: ['.', 'icons'],
  c_name: 'cc_' + cappletname,
  export: true
)

deps = common_deps + [
  colord_dep,
  gnome_rr_dep,
  m_dep,
  upower_glib_dep
]

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir)
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
