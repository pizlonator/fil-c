subdir('appdata')
subdir('completions')

service_conf = configuration_data()
service_conf.set('bindir', control_center_bindir)
service_conf.set('appid', application_id)

service = 'org.gnome.Settings.service'

configure_file(
          input : service + '.in',
         output : '@0@.service'.format(application_id),
        install : true,
    install_dir : join_paths(control_center_datadir, 'dbus-1', 'services'),
  configuration : service_conf
)

desktop_conf = configuration_data()
desktop_conf.set('icon', application_id)

desktop = 'org.gnome.Settings.desktop'

i18n.merge_file(
         type : 'desktop',
        input : configure_file(
            input: desktop + '.in',
            output: 'org.gnome.Settings.desktop.in',
            configuration: desktop_conf
        ),
       output : '@0@.desktop'.format(application_id),
       po_dir : po_dir,
      install : true,
  install_dir : control_center_desktopdir
)

cflags = ['-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)]


# Common sources between gnome-control-center and
# libtestshell.
common_sources = files(
  'cc-application.c',
  'cc-log.c',
  'cc-object-storage.c',
  'cc-panel-loader.c',
  'cc-panel.c',
  'cc-shell.c',
  'cc-panel-list.c',
  'cc-window.c',
)


###################
# Generated files #
###################

generated_sources = files()

#Resources
gresource_file = configure_file(
  input: meson.project_name() + '.gresource.xml.in',
  output: meson.project_name() + '.gresource.xml',
  configuration: service_conf
)

generated_sources += gnome.compile_resources(
  'resources',
  gresource_file,
  export : true,
  dependencies : [gresource_file, appdata_file]
)

common_sources += generated_sources

############
# libshell #
############

libshell = static_library(
               'shell',
              sources : 'cc-shell-model.c',
  include_directories : [top_inc, common_inc],
         dependencies : common_deps,
               c_args : cflags
)

libshell_dep = declare_dependency(
              sources : generated_sources,
  include_directories : top_inc,
            link_with : libshell
)


########################
# gnome-control-center #
########################

shell_sources = common_sources + files('main.c')

shell_deps = common_deps + [
  libdevice_dep,
  liblanguage_dep,
  libwidgets_dep,
  x11_dep,
  libshell_dep,
]

if host_is_linux_not_s390
  shell_deps += wacom_deps
endif

executable(
  meson.project_name(),
         shell_sources,
  include_directories : top_inc,
         dependencies : shell_deps,
               c_args : cflags,
            link_with : panels_libs,
              install : true
)


##################
# lipanel_loader #
##################

# Because it is confusing and somewhat problematic to directly add and compile
# cc-panel-loader.o by another directory (i.e. the shell search provider), we
# have to create a library and link it there, just like libshell.la.
libpanel_loader = static_library(
        'panel_loader',
              sources : 'cc-panel-loader.c',
  include_directories : top_inc,
         dependencies : common_deps,
               c_args : cflags + ['-DCC_PANEL_LOADER_NO_GTYPES']
)


################
# libtestshell #
################

libtestshell = static_library(
           'testshell',
        common_sources,
  include_directories : top_inc,
         dependencies : shell_deps,
               c_args : cflags,
            link_with : panels_libs,
)
libtestshell_dep = declare_dependency(
              sources : generated_sources,
  include_directories : top_inc,
            link_with : libtestshell
)
libtestshell_deps = common_deps + [ libwidgets_dep, libtestshell_dep ]


install_data (
  'org.gnome.Settings.gschema.xml',
  install_dir : control_center_schemadir
)
