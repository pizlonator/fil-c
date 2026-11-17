service_conf = configuration_data()
service_conf.set('libexecdir', control_center_libexecdir)

service = 'org.gnome.Settings.SearchProvider.service'

configure_file(
          input : service + '.in',
         output : service,
        install : true,
    install_dir : join_paths(control_center_datadir, 'dbus-1', 'services'),
  configuration : service_conf
)

install_data(
  'org.gnome.Settings.search-provider.ini',
  install_dir: join_paths(control_center_datadir, 'gnome-shell', 'search-providers')
)

sources = files(
  'cc-search-provider.c',
  'control-center-search-provider.c'
)

# The upstream for the DBus interface definition is
# at http://git.gnome.org/browse/gnome-shell/plain/data/org.gnome.ShellSearchProvider2.xml
sources += gnome.gdbus_codegen(
  'cc-shell-search-provider-generated',
  'org.gnome.ShellSearchProvider2.xml',
  interface_prefix : 'org.gnome.',
         namespace : 'Cc'
)

libs = [
  libpanel_loader,
  libshell
]

executable(
  'gnome-control-center-search-provider',
               sources,
  include_directories : top_inc,
         dependencies : shell_deps,
               c_args : cflags,
            link_with : libs,
              install : true,
          install_dir : control_center_libexecdir
)
