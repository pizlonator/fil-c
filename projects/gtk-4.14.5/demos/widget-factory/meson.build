# demos/widget-factory

if can_use_objcopy_for_resources
  # Create the resource blob
  widgetfactory_gresource = custom_target('widgetfactory.gresource',
      input : 'widget-factory.gresource.xml',
      output : 'widgetfactory.gresource',
      depfile: 'widgetfactory.gresource.d',
      command : [glib_compile_resources,
                 '--generate',
                 '--internal',
                 '--target=@OUTPUT@',
                 '--dependency-file=@DEPFILE@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '@INPUT@'])

  # Create resource data file
  widgetfactory_resources_c = custom_target('widgetfactory_resources.c',
      input : 'widget-factory.gresource.xml',
      output : 'widgetfactory_resources.c',
      depfile: 'widgetfactory_resources.c.d',
      command : [glib_compile_resources,
                 '--generate-source',
                 '--internal',
                 '--target=@OUTPUT@',
                 '--dependency-file=@DEPFILE@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '--external-data',
                 '--c-name', '_g_binary_widgetfactory',
                 '@INPUT@'])

  # Create object file containing resource data
  widgetfactory_resources_binary = custom_target('widgetfactory_resources.o',
      input : widgetfactory_gresource,
      output : 'widgetfactory_resources.o',
      command : [ld,
                 '-z', 'noexecstack',
                 '-r',
                 '-b','binary',
                 '@INPUT@',
                 '-o','@OUTPUT@'])

  # Rename symbol to match the one in the C file
  widgetfactory_resources_o = custom_target('widgetfactory_resources2.o',
    input : widgetfactory_resources_binary,
    output : 'widgetfactory_resources2.o',
    command : [objcopy,
                 '--strip-all',
                 '--add-symbol','_g_binary_widgetfactory_resource_data=.data:0',
                 '@INPUT@',
                 '@OUTPUT@'])

  widgetfactory_resources = [
      widgetfactory_resources_c,
      widgetfactory_resources_o,
    ]
else
  widgetfactory_resources = gnome.compile_resources('widgetfactory_resources',
    'widget-factory.gresource.xml',
    source_dir: meson.current_source_dir(),
  )
endif

executable('gtk4-widget-factory',
  sources: ['widget-factory.c', widgetfactory_resources],
  c_args: common_cflags,
  dependencies: [ libgtk_dep, profile_conf_h ],
  include_directories: confinc,
  win_subsystem: 'windows',
  link_args: extra_demo_ldflags,
  install: true,
)

# desktop file
install_data('org.gtk.WidgetFactory4.desktop', install_dir: gtk_applicationsdir)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach size: ['scalable', 'symbolic']
  install_subdir('data/' + size, install_dir: icontheme_dir)
endforeach

# appdata
configure_file(
  input: 'org.gtk.WidgetFactory4.appdata.xml.in',
  output: 'org.gtk.WidgetFactory4.appdata.xml',
  configuration: appdata_config,
  install_dir: gtk_appdatadir
)
