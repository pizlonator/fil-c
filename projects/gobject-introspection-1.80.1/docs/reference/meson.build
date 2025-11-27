gnome = import('gnome')

version_conf = configuration_data()
version_conf.set('GI_VERSION', meson.project_version())

version_xml = configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: version_conf,
)

ignore_headers = [
  'cmph',
  'girnode.h',
  'girparser.h',
  'girwriter.h',
  'girmodule.h',
  'girepository-private.h',
]

gnome.gtkdoc('gi',
  main_xml: 'gi-docs.xml',
  dependencies: girepo_dep,
  src_dir: 'girepository',
  content_files: [
    'gi-struct-hierarchy.xml',
  ],
  scan_args: [
    '--rebuild-types',
    '--ignore-headers=' + ' '.join(ignore_headers),
  ],
  mkdb_args: [
    '--name-space=g',
    '--ignore-files=cmph',
  ],
  install: true,
)
