glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html', 'glib')

gobject_prefix = gobject_dep.get_pkgconfig_variable('prefix')
gobject_docpath = join_paths(gobject_prefix, 'share', 'gtk-doc', 'html', 'gobject')

libgudev_reference_ignored_h = [
  'gudevenumtypes.h',
]

libgudev_reference_source_folders = [
  join_paths(srcdir, 'gudev'),
]

version_xml = configuration_data()
version_xml.set('VERSION', version)

configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: version_xml,
  install: false,
)

libgudev_reference_scan_args = [
  '--ignore-headers=' + ' '.join(libgudev_reference_ignored_h),
]

libgudev_reference_mkdb_args = [
  '--name-space=g_udev',
]

libgudev_reference_fixxref_args = [
  '--html-dir=@0@'.format(join_paths(gtkdocdir, 'html', package_name)),
  '--extra-dir=@0@'.format(glib_docpath),
  '--extra-dir=@0@'.format(gobject_docpath),
]

gnome.gtkdoc(
  package_name,
  main_xml: 'gudev-docs.xml',
  gobject_typesfile: files('gudev.types'),
  src_dir: libgudev_reference_source_folders,
  dependencies: libgudev_dep,
  scan_args: libgudev_reference_scan_args,
  mkdb_args: libgudev_reference_mkdb_args,
  fixxref_args: libgudev_reference_fixxref_args,
  install: true,
  install_dir: join_paths(
    gtkdocdir,
    'html',
    package_name,
  )
)