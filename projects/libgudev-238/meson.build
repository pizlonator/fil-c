project('libgudev', 'c',
  version: '238',
  license: 'LGPLv2.1+',
  meson_version: '>= 0.53.0',
  default_options: [
    'buildtype=debugoptimized',
  ]
)

gnome = import('gnome')
pkg = import('pkgconfig')

# Versioning
api_version = '1.0'
lib_version = '0.3.0'
version = meson.project_version()

package_name = 'gudev'
package_string = '@0@-@1@'.format(package_name, api_version)

# Paths
rootdir = include_directories('.')

srcdir = meson.source_root()
builddir = meson.build_root()

prefix = get_option('prefix')

includedir = join_paths(prefix, get_option('includedir'))
libdir = join_paths(prefix, get_option('libdir'))
datadir = join_paths(prefix, get_option('datadir'))

pkgincludedir = join_paths(includedir, package_string)
typelibdir = join_paths(libdir, 'girepository-1.0')
pkgconfigdir = join_paths(libdir, 'pkgconfig')
girdir = join_paths(datadir, 'gir-1.0')
gtkdocdir = join_paths(datadir, 'gtk-doc')
vapidir = join_paths(datadir, 'vala', 'vapi')

# Dependencies
cc = meson.get_compiler('c')

glib_req = '>= 2.38.0'
libudev_req = '>= 251'

glib_dep = dependency('glib-2.0', version: glib_req)
gobject_dep = dependency('gobject-2.0', version: glib_req)
libudev_dep = dependency('libudev', version: libudev_req)

gio_dep = dependency(
  'gio-2.0', version: glib_req,
  required: get_option('tests'))
umockdev_dep = dependency(
  'umockdev-1.0',
  required: get_option('tests')
)

gir_dep = find_program('g-ir-scanner', required: get_option('introspection'))
vapigen_dep = find_program('vapigen', required : get_option('vapi'))

if vapigen_dep.found()
  assert(gir_dep.found(), 'vapi requires introspection')
endif

# Configurations
config_h = configuration_data()
config_h.set_quoted('VERSION', version)

configure_file(
  output: 'config.h',
  configuration: config_h
)

top_srcdir = meson.current_source_dir()

subdir('gudev')

if umockdev_dep.found()
  subdir('tests')
endif

if get_option('gtk_doc')
  subdir('docs')
endif

summary('Documentation', get_option('gtk_doc'), section: 'Build')
summary('Introspection', gir_dep.found(), section: 'Build')
summary('Vala API', vapigen_dep.found(), section: 'Build')
summary('Tests', umockdev_dep.found(), section: 'Build')

summary('prefix', prefix, section: 'Directories')
summary('includedir', includedir, section: 'Directories')
summary('libdir', libdir, section: 'Directories')
summary('datadir', datadir, section: 'Directories')

