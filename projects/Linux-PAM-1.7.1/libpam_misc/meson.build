subdir('include/security')

libpam_misc_src = [
  'help_env.c',
  'misc_conv.c',
]

libpam_misc_inc = include_directories('include')

libpam_misc_version = '0.82.1'
libpam_misc_map = 'libpam_misc.map'
libpam_misc_map_path = meson.current_source_dir() / libpam_misc_map

libpam_misc_link_deps = [libpam_misc_map]
libpam_misc_link_args = ['-Wl,--version-script=' + libpam_misc_map_path]

libpam_misc = shared_library(
  'pam_misc',
  sources: libpam_misc_src,
  include_directories: [libpam_misc_inc, libpamc_inc],
  dependencies: [libpam_internal_dep, libpam_dep],
  link_depends: libpam_misc_link_deps,
  link_args: libpam_misc_link_args,
  version: libpam_misc_version,
  install: true,
)

libpam_misc_dep = declare_dependency(
  include_directories: [libpam_misc_inc],
  link_with: [libpam_misc],
)

pkgconfig.generate(
  libpam_misc,
  description: 'Miscellaneous functions that make the job of writing PAM-aware applications easier.',
  name: 'pam_misc',
  filebase: 'pam_misc',
  version: meson.project_version(),
  url: 'http://www.linux-pam.org/'
)
