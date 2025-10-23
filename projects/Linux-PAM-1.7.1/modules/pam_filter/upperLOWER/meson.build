executable(
  'upperLOWER',
  sources: ['upperLOWER.c'],
  include_directories: ['..'],
  link_args: exe_link_args,
  dependencies: [libpam_internal_dep, libpam_dep],
  install: true,
  install_dir: securedir / 'pam_filter',
)
