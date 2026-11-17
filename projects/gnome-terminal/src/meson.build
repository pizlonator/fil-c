# Copyright © 2019, 2020, 2021, 2022 Christian Persch
#
# This programme is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This programme is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this programme.  If not, see <https://www.gnu.org/licenses/>.

src_inc = include_directories('.')

# UI files

ui_xsltproc_command = [
  xsltproc,
  '--nonet',
  '-o', '@OUTPUT@',
  '@INPUT@',
]

menubar_ui_mnemonics = custom_target(
  'terminal-menubar-with-mnemonics.ui',
  command: ui_xsltproc_command,
  input: files(
    '..' / 'data' / 'ui-filter-mnemonics.xslt',
    'terminal-menubar.ui.in',
  ),
  install: false,
  output: 'terminal-menubar-with-mnemonics.ui',
)

menubar_ui_nomnemonics = custom_target(
  'terminal-menubar-without-mnemonics.ui',
  command: ui_xsltproc_command,
  input: files(
    '..' / 'data' / 'ui-filter-no-mnemonics.xslt',
    'terminal-menubar.ui.in',
  ),
  install: false,
  output: 'terminal-menubar-without-mnemonics.ui',
)

# Common sources

app_sources = files(
  'terminal-accels.cc',
  'terminal-accels.hh',
  'terminal-app.cc',
  'terminal-app.hh',
)

client_util_sources = files(
  'terminal-client-utils.cc',
  'terminal-client-utils.hh',
)

debug_sources = files(
  'terminal-debug.cc',
  'terminal-debug.hh',
)

default_sources = files(
  'terminal-default.cc',
  'terminal-default.hh',
)

dbus_sources = gnome.gdbus_codegen(
  'terminal-gdbus-generated',
  gt_dns_name + '.xml',
  autocleanup: 'none',
  install_header: false,
  interface_prefix: gt_dns_name,
  namespace: 'Terminal',
  object_manager: true,
)

egg_sources = files(
  'eggshell.cc',
  'eggshell.hh',
)

i18n_sources = files(
  'terminal-i18n.cc',
  'terminal-i18n.hh',
  'terminal-intl.hh',
)

marshal_sources = gnome.genmarshal(
  'terminal-marshal',
  internal: true,
  install_header: false,
  prefix: '_terminal_marshal',
  sources: files(
    'terminal-marshal.list',
  ),
  stdinc: true,
  valist_marshallers: true,
)

misc_sources = files(
  'terminal-defines.hh',
  'terminal-libgsystem.hh',
)

profiles_sources = files(
  'terminal-profiles-list.cc',
  'terminal-profiles-list.hh',
  'terminal-schemas.hh',
  'terminal-settings-list.cc',
  'terminal-settings-list.hh',
)

settings_dbus_sources = gnome.gdbus_codegen(
  'terminal-settings-bridge-generated',
  'org.gnome.Terminal.SettingsBridge.xml',
  autocleanup: 'all',
  install_header: false,
  interface_prefix: gt_dns_name,
  namespace: 'Terminal',
  object_manager: false,
)

settings_utils_sources = files(
  'terminal-settings-utils.cc',
  'terminal-settings-utils.hh',
)

regex_sources = files(
  'terminal-regex.hh',
)

types_sources = files(
  'terminal-enums.hh',
)

types_sources += gnome.mkenums(
  'terminal-type-builtins',
  c_template: 'terminal-type-builtins.hh.template',
  h_template: 'terminal-type-builtins.cc.template',
  install_header: false,
  sources: types_sources,
)

version_conf = {
  'TERMINAL_MAJOR_VERSION': gt_major_version.to_string(),
  'TERMINAL_MICRO_VERSION': gt_micro_version.to_string(),
  'TERMINAL_MINOR_VERSION': gt_minor_version.to_string(),
}

version_sources = [configure_file(
  configuration: version_conf,
  input: 'terminal-version.hh.in',
  install: false,
  output: '@BASENAME@',
)]

util_sources = files(
  'terminal-util.cc',
  'terminal-util.hh',
)

# Flags

common_cxxflags = version_cxxflags + [
  '-DTERMINAL_COMPILATION',
  '-DTERM_PREFIX="@0@"'.format(gt_prefix),
  '-DTERM_BINDIR="@0@"'.format(gt_prefix / gt_bindir),
  '-DTERM_DATADIR="@0@"'.format(gt_prefix / gt_datadir),
  '-DTERM_LIBEXECDIR="@0@"'.format(gt_prefix / gt_libexecdir),
  '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
  '-DTERM_PKGDATADIR="@0@"'.format(gt_prefix / gt_pkgdatadir),
  '-DTERM_PKGLIBDIR="@0@"'.format(gt_prefix / gt_pkglibdir),
  '-DVTE_DISABLE_DEPRECATION_WARNINGS',
]

# Server

server_resources_sources = gnome.compile_resources(
  'terminal-server-resources',
  'terminal.gresource.xml',
  c_name: 'terminal',
  dependencies: [
    menubar_ui_mnemonics,
    menubar_ui_nomnemonics,
  ],
  export: false,
  install: false,
)

server_sources = app_sources + client_util_sources + debug_sources + default_sources + dbus_sources + egg_sources + i18n_sources + marshal_sources + misc_sources + profiles_sources + regex_sources + server_resources_sources + settings_dbus_sources + settings_utils_sources + types_sources + util_sources + version_sources + files(
  'server.cc',
  'terminal-enums.hh',
  'terminal-gdbus.cc',
  'terminal-gdbus.hh',
  'terminal-headerbar.cc',
  'terminal-headerbar.hh',
  'terminal-icon-button.cc',
  'terminal-icon-button.hh',
  'terminal-info-bar.cc',
  'terminal-info-bar.hh',
  'terminal-mdi-container.cc',
  'terminal-mdi-container.hh',
  'terminal-menu-button.cc',
  'terminal-menu-button.hh',
  'terminal-notebook.cc',
  'terminal-notebook.hh',
  'terminal-pcre2.hh',
  'terminal-prefs-process.cc',
  'terminal-prefs-process.hh',
  'terminal-screen-container.cc',
  'terminal-screen-container.hh',
  'terminal-screen.cc',
  'terminal-screen.hh',
  'terminal-search-popover.cc',
  'terminal-search-popover.hh',
  'terminal-settings-bridge-impl.cc',
  'terminal-settings-bridge-impl.hh',
  'terminal-tab-label.cc',
  'terminal-tab-label.hh',
  'terminal-window.cc',
  'terminal-window.hh',
)

if get_option('search_provider')

  server_sources += files(
    'terminal-search-provider.cc',
    'terminal-search-provider.hh',
  )

  search_provider_dbus_interface = files(
    'org.gnome.ShellSearchProvider2.xml',
  )

  server_sources += gnome.gdbus_codegen(
    'terminal-search-provider-gdbus-generated',
    search_provider_dbus_interface,
    autocleanup: 'none',
    install_header: false,
    interface_prefix: 'org.gnome.Shell',
    namespace: 'Terminal',
  )

endif

server_incs = [
  top_inc,
  src_inc,
]

server_cxxflags = common_cxxflags + [
  '-DTERMINAL_SERVER',
]

server_deps = [
  gio_dep,
  gio_unix_dep,
  glib_dep,
  gtk_dep,
  libdl_dep,
  pcre2_dep,
  pthreads_dep,
  schemas_dep,
  uuid_dep,
  vte_dep,
  x11_dep,
]

server = executable(
  gt_name + '-server',
  cpp_args: server_cxxflags,
  dependencies: server_deps,
  include_directories: server_incs,
  install: true,
  install_dir: gt_prefix / gt_libexecdir,
  install_tag: 'runtime',
  sources: server_sources,
)

# systemd and D-Bus glue

server_conf = configuration_data()
server_conf.set('gt_name', gt_name)
server_conf.set('gt_dns_name', gt_dns_name)
server_conf.set('libexecdir', gt_prefix / gt_libexecdir)

configure_file(
  input: gt_dns_name + '.service.in',
  output: gt_dns_name + '.service',
  configuration: server_conf,
  install: true,
  install_dir: gt_prefix / gt_dbusservicedir,
  install_tag: 'runtime',
)

configure_file(
  input: gt_name + '-server.service.in',
  output: gt_name + '-server.service',
  configuration: server_conf,
  install: true,
  install_dir: gt_prefix / gt_systemduserdir,
  install_tag: 'runtime',
)

# Search provider

if get_option('search_provider')

  provider_ini = files(gt_name + '-search-provider.ini')

  install_data(
    provider_ini,
    install_dir: gt_prefix / gt_searchproviderdir,
    install_tag: 'runtime',
  )
endif # option 'search_provider'

# Preferences

prefs_resources_sources = gnome.compile_resources(
  'terminal-prefs-resources',
  'prefs.gresource.xml',
  c_name: 'terminal',
  export: false,
  install: false,
)

prefs_main_sources = app_sources + client_util_sources + debug_sources + default_sources + i18n_sources + marshal_sources + misc_sources + prefs_resources_sources + profiles_sources + settings_dbus_sources + settings_utils_sources + types_sources + util_sources + version_sources + files(
  'prefs-main.cc',
  'profile-editor.cc',
  'profile-editor.hh',
  'terminal-prefs.cc',
  'terminal-prefs.hh',
  'terminal-settings-bridge-backend.cc',
  'terminal-settings-bridge-backend.hh',
)

prefs_main_cxxflags = common_cxxflags + [
  '-DTERMINAL_PREFERENCES',
]

prefs_main_incs = server_incs
prefs_main_deps = server_deps

prefs_main = executable(
  gt_name + '-preferences',
  cpp_args: prefs_main_cxxflags,
  include_directories: prefs_main_incs,
  dependencies: prefs_main_deps,
  install: true,
  install_dir: gt_prefix / gt_libexecdir,
  install_tag: 'runtime',
  sources: prefs_main_sources,
)

# Legacy client

client_sources = client_util_sources + debug_sources + dbus_sources + i18n_sources + profiles_sources + settings_utils_sources + types_sources + files(
  'terminal-options.cc',
  'terminal-options.hh',
  'terminal.cc',
)

client_incs = [
  top_inc,
  src_inc,
]

client_incs = [
  top_inc,
  src_inc,
]

client_cxxflags = common_cxxflags + [
  '-DTERMINAL_CLIENT',
]

client_deps = [
  gio_dep,
  gio_unix_dep,
  glib_dep,
  gtk_dep,
  x11_dep,
  uuid_dep,
  vte_dep,
]

client = executable(
  gt_name,
  cpp_args: client_cxxflags,
  include_directories: client_incs,
  dependencies: client_deps,
  install: true,
  install_tag: 'runtime',
  sources: client_sources,
)

# Settings schemas

install_data(
  gt_dns_name + '.gschema.xml',
  install_dir: gt_prefix / gt_schemadir,
  install_tag: 'runtime',
)

meson.add_install_script(
  'meson_compileschemas.py',
  gt_prefix / gt_schemadir,
)

reference_schemas = custom_target(
  'gschemas.compiled',
  command: [
    glib_compile_schemas,
    '--targetdir', meson.current_build_dir(),
    '--schema-file', files(gt_dns_name + '.gschema.xml'),
    '--schema-file', files('external.gschema.xml'),
  ],
  install: true,
  install_dir: gt_prefix / gt_pkglibdir,
  install_tag: 'runtime',
  output: 'gschemas.compiled',
)

# Nautilus extension

if get_option('nautilus_extension')

  nautilus_sources = client_util_sources + dbus_sources + i18n_sources + misc_sources + types_sources + files(
    'terminal-nautilus.cc',
  )

  nautilus_map = meson.current_source_dir() / 'nautilus.map'

  nautilus_incs = [
    top_inc,
    src_inc,
  ]

  nautilus_cxxflags = glib_version_cxxflags + [
    '-DTERMINAL_NAUTILUS',
    '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
  ]

  nautilus_deps = [
    gio_dep,
    gio_unix_dep,
    glib_dep,
    libnautilus_extension_dep,
  ]

  nautilus_ldflags = [
    '-Wl,--version-script,@0@'.format(nautilus_map),
  ]

  nautilus_extension = shared_library(
     'terminal-nautilus',
     cpp_args: nautilus_cxxflags,
     dependencies: nautilus_deps,
     include_directories: nautilus_incs,
     install: true,
     install_dir: gt_prefix / gt_nautilusextensiondir,
     install_tag: 'runtime',
     link_args: nautilus_ldflags,
     link_depends: nautilus_map,
     sources: nautilus_sources,
     soversion: '',
  )

endif # option 'nautilus_extension'

# Unit tests

test_regex_sources = regex_sources + files(
  'terminal-regex.cc',
)

test_regex = executable(
  'test-regex',
  cpp_args: [
    '-DTERMINAL_REGEX_MAIN',
  ],
  dependencies: [
    glib_dep,
    pcre2_dep,
  ],
  include_directories: [top_inc, src_inc,],
  sources: test_regex_sources,
  install: false,
)

test_env = [
  'GNOME_TERMINAL_DEBUG=0',
  'VTE_DEBUG=0',
]

test_units = [
  ['regex', test_regex],
]

foreach test: test_units
  test(
    test[0],
    test[1],
    env: test_env,
  )
endforeach

# Non-unit tests

test_default_sources = debug_sources + default_sources + misc_sources

test_default = executable(
  'test-default',
  cpp_args: common_cxxflags + [
    '-DTERMINAL_DEFAULT_MAIN',
  ],
  dependencies: [
    glib_dep,
  ],
  include_directories: [top_inc, src_inc,],
  sources: test_default_sources,
  install: false,
)
