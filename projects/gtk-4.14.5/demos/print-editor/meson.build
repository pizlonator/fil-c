executable('gtk4-print-editor',
  sources: ['print-editor.c'],
  c_args: common_cflags,
  dependencies: [ libgtk_dep, profile_conf_h ],
  include_directories: confinc,
  win_subsystem: 'windows',
  link_args: extra_demo_ldflags,
  install: true,
)

# desktop file
install_data('org.gtk.PrintEditor4.desktop', install_dir: gtk_applicationsdir)

# appdata
install_data('org.gtk.PrintEditor4.appdata.xml', install_dir: gtk_appdatadir)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach size: ['scalable', 'symbolic']
  install_subdir('data/' + size, install_dir: icontheme_dir)
endforeach
