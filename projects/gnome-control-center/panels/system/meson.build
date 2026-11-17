panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

polkit = 'org.gnome.controlcenter.user-accounts.policy'.format(cappletname)
i18n.merge_file(
  input: 'users/' + polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

install_data(
  'org.gnome.Settings-system-symbolic.svg',
  install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'apps')
)

sources = files(
  'cc-password-utils.c',
  'cc-system-panel.c',
  'about/cc-about-page.c',
  'about/cc-system-details-window.c',
  'about/cc-info-entry.c',
  'about/info-cleanup.c',
  'datetime/cc-datetime-page.c',
  'datetime/cc-tz-item.c',
  'datetime/cc-tz-dialog.c',
  'datetime/date-endian.c',
  'datetime/tz.c',
  'region/cc-region-page.c',
  'region/cc-format-chooser.c',
  'region/cc-format-preview.c',
  'remote-desktop/cc-desktop-sharing-page.c',
  'remote-desktop/cc-encryption-fingerprint-dialog.c',
  'remote-desktop/cc-gnome-remote-desktop.c',
  'remote-desktop/cc-remote-desktop-page.c',
  'remote-desktop/cc-remote-session-page.c',
  'remote-desktop/cc-tls-certificate.c',
  'remote-desktop/cc-systemd-service.c',
  'remote-login/cc-remote-login.c',
  'remote-login/cc-remote-login-page.c',
  'remote-login/cc-systemd-service.c',
  'users/cc-add-user-dialog.c',
  'users/cc-avatar-chooser.c',
  'users/cc-crop-area.c',
  'users/cc-entry-feedback.c',
  'users/cc-enterprise-login-dialog.c',
  'users/cc-fingerprint-dialog.c',
  'users/cc-fingerprint-manager.c',
  'users/cc-password-dialog.c',
  'users/cc-realm-manager.c',
  'users/cc-user-page.c',
  'users/cc-users-page.c',
  'users/pw-utils.c',
  'users/run-passwd.c',
  'users/user-utils.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name : 'cc_' + cappletname,
  export : true
)

enums_header = files(
  gsettings_desktop_dep.get_variable(pkgconfig: 'prefix') + '/include/gsettings-desktop-schemas/gdesktop-enums.h',
  'datetime/cc-datetime-page.h'
)

enums = 'gdesktop-enums-types'
sources += gnome.mkenums(
  enums + '.h',
  sources: enums_header,
  fhead: '#pragma once\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define G_DESKTOP_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n'
)

sources += gnome.mkenums(
  enums + '.c',
  sources: enums_header,
  fhead: '#include <gdesktop-enums.h>\n#include "gdesktop-enums-types.h"\n#include "datetime/cc-datetime-page.h"',
  fprod: '\n/* enumerations from "@filename@" */',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {',
  vprod: '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static ("@EnumName@", values);\n  }\n  return etype;\n}\n'
)

libsecret_dep = dependency('libsecret-1')
gnutls_dep = dependency('gnutls')
libgtop2 = dependency('libgtop-2.0')
udisks2 = dependency('udisks2', version: '>= 2.8.2')

# Kerberos support
krb_dep = dependency('krb5', required: false)
assert(krb_dep.found(), 'kerberos libraries not found in your path')

deps = common_deps + [
  accounts_dep,
  gcr_dep,
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  gnutls_dep,
  gudev_dep,
  libgtop2,
  liblanguage_dep,
  libsecret_dep,
  krb_dep,
  polkit_gobject_dep,
  pwquality_dep,
  udisks2,
]

if enable_ibus
  deps += ibus_dep
endif

if enable_malcontent
  deps += malcontent_dep
endif

settings_daemon = 'org.gnome.SettingsDaemon'
gsd_gdbus = settings_daemon + '.Sharing'

sources += gnome.gdbus_codegen(
  gsd_gdbus,
  gsd_gdbus + '.xml',
  interface_prefix: settings_daemon + '.',
  namespace: 'Gsd'
)

cflags += [
   '-DDATADIR="@0@"'.format(control_center_datadir),
   '-DSYSCONFDIR="@0@"'.format(control_center_sysconfdir),
   '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
  '-DGNOMECC_DATA_DIR="@0@"'.format(control_center_pkgdatadir)
]

subdir('about')
subdir('datetime')
subdir('region')
subdir('remote-desktop')
subdir('remote-login')
subdir('users')

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc, include_directories('.'), include_directories('users')],
  dependencies: deps,
  c_args: cflags
)
