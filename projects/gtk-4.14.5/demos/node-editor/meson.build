node_editor_sources = [
  'gtkrendererpaintable.c',
  'main.c',
  'node-editor-application.c',
  'node-editor-window.c',
]

node_editor_resources = gnome.compile_resources('node_editor_resources',
  'node-editor.gresource.xml',
  source_dir: meson.current_source_dir(),
)

executable('gtk4-node-editor',
  sources: [node_editor_sources, node_editor_resources],
  dependencies: [ libgtk_dep, profile_conf_h ],
  include_directories: confinc,
  c_args: common_cflags,
  win_subsystem: 'windows',
  link_args: extra_demo_ldflags,
  install: true,
)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach size: ['scalable', 'symbolic']
  install_subdir('data/' + size, install_dir: icontheme_dir)
endforeach

# desktop file
install_data('org.gtk.gtk4.NodeEditor.desktop', install_dir: gtk_applicationsdir)

# appdata
configure_file(
  input: 'org.gtk.gtk4.NodeEditor.appdata.xml.in',
  output: 'org.gtk.gtk4.NodeEditor.appdata.xml',
  configuration: appdata_config,
  install_dir: gtk_appdatadir
)

