iconbrowser_sources = [
  'main.c',
  'iconbrowserapp.c',
  'iconbrowserwin.c',
  'iconbrowsericon.c',
  'iconbrowsercontext.c'
]

iconbrowser_resources = gnome.compile_resources('iconbrowser_resources',
  'iconbrowser.gresource.xml',
  source_dir: meson.current_source_dir(),
)

executable('gtk4-icon-browser',
  sources: [iconbrowser_sources, iconbrowser_resources],
  c_args: common_cflags,
  dependencies: [ libgtk_dep, profile_conf_h ],
  include_directories: confinc,
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
install_data('org.gtk.IconBrowser4.desktop', install_dir: gtk_applicationsdir)

# appdata
configure_file(
  input: 'org.gtk.IconBrowser4.appdata.xml.in',
  output: 'org.gtk.IconBrowser4.appdata.xml',
  configuration: appdata_config,
  install_dir: gtk_appdatadir
)

