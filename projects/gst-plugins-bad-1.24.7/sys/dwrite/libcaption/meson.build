libcaption_sources = [
  'caption.c',
  'eia608_charmap.c',
  'eia608_from_utf8.c',
  'eia608.c',
  'utf8.c',
  'xds.c',
]

libcaption_static = static_library('libcaption-static',
  libcaption_sources,
  include_directories : include_directories('.'),
  override_options: ['werror=false'],
)

dwrite_libcaption_dep = declare_dependency(
  link_with : libcaption_static,
  include_directories: include_directories('.')
)
