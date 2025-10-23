subdir('include/security')

libpamc_src = [
  'pamc_client.c',
  'pamc_converse.c',
  'pamc_load.c',
]

libpamc_inc = include_directories('include')

libpamc_version = '0.82.1'
libpamc_map = 'libpamc.map'
libpamc_map_path = meson.current_source_dir() / libpamc_map

libpamc_link_deps = [libpamc_map]
libpamc_link_args = ['-Wl,--version-script=' + libpamc_map_path]

libpamc = shared_library(
  'pamc',
  sources: libpamc_src,
  include_directories: [libpam_inc, libpamc_inc],
  dependencies: [libpam_internal_dep],
  link_depends: libpamc_link_deps,
  link_args: libpamc_link_args,
  version: libpamc_version,
  install: true,
)

pkgconfig.generate(
  libpamc,
  description: 'The PAM client API library and binary prompt support. Rarely used.',
  name: 'libpamc',
  filebase: 'pamc',
  version: meson.project_version(),
  url: 'http://www.linux-pam.org/'
)
