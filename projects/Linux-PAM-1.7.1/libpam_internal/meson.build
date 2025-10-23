libpam_internal_src = [
  'pam_debug.c',
  'pam_econf.c',
  'pam_line.c',
]

libpam_internal_inc = include_directories('include')

libpam_internal = static_library(
  'pam_internal',
  sources: libpam_internal_src,
  include_directories: [libpam_internal_inc, libpam_inc],
  dependencies: libeconf,
)

libpam_internal_dep = declare_dependency(
  include_directories: [libpam_internal_inc],
  link_with: [libpam_internal],
)
