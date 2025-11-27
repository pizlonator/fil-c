project('gi-sample-meson', 'c')

libsources = ['gi-sample.c', 'gi-sample.h']

gnome = import('gnome')
gobj_dep = dependency('gobject-2.0')

girlib = shared_library(
  'gi-sample',
  sources : libsources,
  dependencies : [gobj_dep],
  install : true
)

gnome.generate_gir(
  girlib,
  sources : libsources,
  nsversion : '1.0',
  namespace : 'GISample',
  symbol_prefix : 'gi_sample',
  identifier_prefix : 'GISample',
  includes : ['GObject-2.0'],
  install : true,
)
