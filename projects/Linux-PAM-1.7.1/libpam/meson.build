subdir('include')

libpam_src = [
  'pam_account.c',
  'pam_audit.c',
  'pam_auth.c',
  'pam_data.c',
  'pam_delay.c',
  'pam_dispatch.c',
  'pam_dynamic.c',
  'pam_end.c',
  'pam_env.c',
  'pam_get_authtok.c',
  'pam_handlers.c',
  'pam_item.c',
  'pam_misc.c',
  'pam_modutil_check_user.c',
  'pam_modutil_cleanup.c',
  'pam_modutil_getgrgid.c',
  'pam_modutil_getgrnam.c',
  'pam_modutil_getlogin.c',
  'pam_modutil_getpwnam.c',
  'pam_modutil_getpwuid.c',
  'pam_modutil_getspnam.c',
  'pam_modutil_ingroup.c',
  'pam_modutil_ioloop.c',
  'pam_modutil_priv.c',
  'pam_modutil_sanitize.c',
  'pam_modutil_searchkey.c',
  'pam_password.c',
  'pam_prelude.c',
  'pam_session.c',
  'pam_start.c',
  'pam_strerror.c',
  'pam_syslog.c',
  'pam_vprompt.c',
]

libpam_version = '0.85.1'
libpam_map = 'libpam.map'
libpam_map_path = meson.current_source_dir() / libpam_map

libpam_link_deps = [libpam_map]
libpam_link_args = ['-Wl,--version-script=' + libpam_map_path]

libpam = shared_library(
  'pam',
  sources: libpam_src,
  include_directories: [libpam_inc],
  c_args: [
    '-DDEFAULT_MODULE_PATH="@0@/"'.format(securedir),
    '-DLIBPAM_COMPILE',
  ],
  dependencies: [libpam_internal_dep, libaudit, libeconf, libdl, libintl],
  link_depends: libpam_link_deps,
  link_args: libpam_link_args,
  version: libpam_version,
  install: true,
)

libpam_dep = declare_dependency(
  include_directories: [libpam_inc],
  link_with: [libpam],
)

pkgconfig.generate(
  libpam,
  description: 'The primary Linux-PAM library. It is used by PAM modules and PAM-aware applications.',
  name: 'PAM',
  filebase: 'pam',
  version: meson.project_version(),
  url: 'http://www.linux-pam.org/'
)
