project(
  'gnome-control-center', 'c',
        version : '46.8',
        license : 'GPL2+',
  meson_version : '>= 0.58.0'
)

control_center_prefix = get_option('prefix')
control_center_bindir = join_paths(control_center_prefix, get_option('bindir'))
control_center_datadir = join_paths(control_center_prefix, get_option('datadir'))
control_center_libexecdir = join_paths(control_center_prefix, get_option('libexecdir'))
control_center_localedir = join_paths(control_center_prefix, get_option('localedir'))
control_center_mandir = join_paths(control_center_prefix, get_option('mandir'))
control_center_sysconfdir = join_paths(control_center_prefix, get_option('sysconfdir'))

control_center_pkgdatadir = join_paths(control_center_datadir, meson.project_name())
control_center_desktopdir = join_paths(control_center_datadir, 'applications')
control_center_icondir = join_paths(control_center_datadir, 'icons')
control_center_schemadir = join_paths (control_center_datadir, 'glib-2.0', 'schemas')

control_center_gettext = meson.project_name() + '-2.0'

host_is_linux = host_machine.system().contains('linux')
host_is_linux_not_s390 = host_is_linux and not host_machine.cpu().contains('s390')

cc = meson.get_compiler('c')

config_h = configuration_data()

py = import('python')
python = py.find_installation('python3')

config_h.set_quoted('TEST_NM_PYTHON', python.full_path())

if get_option('profile') == 'development'
    profile = 'Devel'
else
    profile = ''
endif
application_id = 'org.gnome.Settings@0@'.format(profile)

version_split = meson.project_version().split('.')

# defines
set_defines = [
  # package
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_VERSION', meson.project_version()],
  ['VERSION', meson.project_version()],
  ['MAJOR_VERSION', version_split[0]],
  ['PROFILE', get_option('profile')],
  # i18n
  ['GETTEXT_PACKAGE', control_center_gettext]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

distributor_logo = get_option('distributor_logo')
if (distributor_logo != '')
  config_h.set_quoted('DISTRIBUTOR_LOGO', distributor_logo,
                      description: 'Define to absolute path of distributor logo')
  dark_mode_distributor_logo = get_option('dark_mode_distributor_logo')
  if (dark_mode_distributor_logo != '')
    config_h.set_quoted('DARK_MODE_DISTRIBUTOR_LOGO', dark_mode_distributor_logo,
                        description: 'Define to absolute path of distributor logo for use in dark mode')
  endif
endif

# meson does not support octal values, so it must be handled as a
# string. See: https://github.com/mesonbuild/meson/issues/2047
config_h.set('USER_DIR_MODE', '0700',
             description: 'Permissions for creating the user\'s config, cache and data directories')

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']

# We have so many deprecation warnings that it becomes impossible to notice any other useful
# warning, thus making the compiler output completely useless. Ideally, we should fix all
# of those but, until then, it's better to add this flag so that we avoid adding even more
# warnings by accident. If you want to fix some of those, just build Settings with:
# meson _build -Ddeprecated-declarions=enabled
if get_option('deprecated-declarations').disabled()
  common_flags += '-Wno-deprecated-declarations'
endif

# Only add this when optimizing is enabled (default)
optimized_src = '''
  #if __OPTIMIZE__ == 0
  #error No optimization
  #endif
'''

control_center_optimized = get_option('buildtype').contains('optimized') and cc.compiles(optimized_src)

if control_center_optimized
  common_flags += '-Wp,-D_FORTIFY_SOURCE=2'
endif

if get_option('buildtype').contains('debug')
  test_cflags = [
    '-Wcast-align',
    '-Wmissing-field-initializers',
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wno-strict-aliasing',
    '-Wno-sign-compare'
  ]

  common_flags += cc.get_supported_arguments(test_cflags)
endif

add_project_arguments(common_flags, language: 'c')

# Check that we meet the dependencies
libgvc = subproject(
  'gvc',
  default_options: [
    'static=true',
    'alsa=false',
    'package_name=' + control_center_gettext,
    'package_version=' + meson.project_version(),
  ]
)
libgvc_dep = libgvc.get_variable('libgvc_dep')

goa_req_version = '>= 3.49.1'
pulse_req_version = '>= 2.0'

gtk_dep = dependency(
  'gtk4',
  version: '>= 4.11.2',
  fallback: ['gtk', 'gtk_dep'],
  default_options: [
    'introspection=disabled',
    'demos=false',
    'build-testsuite=false',
    'build-tests=false',
    'build-examples=false',
  ]
)

libadwaita_dep = dependency(
  'libadwaita-1',
  version: '>= 1.4.alpha',
  required: false
)
# FIXME: this is a workaround because interactive-tests don't work with libadwaita as a subproject. See !1754
libadwaita_is_subproject = not libadwaita_dep.found()
if libadwaita_is_subproject
  libadwaita = subproject(
    'libadwaita',
    default_options: [
      'examples=false',
      'introspection=disabled',
      'tests=false',
      'vapi=false',
    ],
  )
  libadwaita_dep = libadwaita.get_variable('libadwaita_dep')
endif

tecla_dep = dependency(
  'tecla',
  required: false
)

# Fil-C: tecla (the keyboard shortcut previewer) is not part of the
# pizlonated system, and its wrap file would require a network git clone at
# configure time.  Reference the (optional) installed tecla binary instead:
# the keyboard panel keeps working without it (the shortcut preview button
# simply has nothing to launch).
tecla_is_subproject = true
if not tecla_dep.found()
  tecla_dep = dependency('', required: false)
endif

accounts_dep = dependency('accountsservice', version: '>= 0.6.39')
colord_dep = dependency('colord', version: '>= 0.1.34')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.23.0')
gio_dep = dependency('gio-2.0')
glib_dep = dependency('glib-2.0', version: '>= 2.76.6')
gnome_desktop_dep = dependency('gnome-desktop-4')
gnome_bg_dep = dependency('gnome-bg-4')
gnome_rr_dep = dependency('gnome-rr-4')
gnome_settings_dep = declare_dependency(
  # Fil-C: gnome-settings-daemon is not part of the pizlonated system; the
  # gsd-enums.h header the panels include is vendored under
  # panels/common/gsd-include/gnome-settings-daemon/gsd-enums.h.
  include_directories: include_directories('panels/common/gsd-include'),
)
goa_dep = dependency('goa-1.0', version: goa_req_version, required: false)
# Fil-C: gnome-online-accounts is not part of the pizlonated system; the
# online-accounts panel is dropped (see panels/meson.build) and the panel
# loader entry is guarded by HAVE_GOA below.
gsettings_desktop_dep = dependency('gsettings-desktop-schemas', version: '>= 46.beta')
libxml_dep = dependency('libxml-2.0')
pulse_dep = dependency('libpulse', version: pulse_req_version)
pulse_mainloop_dep = dependency('libpulse-mainloop-glib', version: pulse_req_version)
upower_glib_dep = dependency('upower-glib', version: '>= 0.99.8')
gudev_dep = dependency('gudev-1.0', version: '>= 232')
x11_dep = dependency('x11', version: '>= 1.8')
xi_dep = dependency('xi', version: '>= 1.2')
epoxy_dep = dependency('epoxy')
gcr_dep = dependency('gcr-4', version: '>= 4.1.0')
# Fil-C: libpwquality is not part of the pizlonated system; the system panel
# (which needs it for the users subpage) is dropped.
pwquality_dep = dependency('pwquality', version: '>= 1.2.2', required: false)

# Fil-C: optional panel dependencies.  The corresponding panels are dropped
# (see panels/meson.build) when the dependency is missing, and the panel
# loader entries (shell/cc-panel-loader.c) are guarded by the same macros.
libgtop_dep = dependency('libgtop-2.0', required: false)
if libgtop_dep.found()
  config_h.set('HAVE_LIBGTOP', 1,
               description: 'Define if libgtop is available')
endif
udisks_dep = dependency('udisks2', version: '>= 2.8.2', required: false)
if udisks_dep.found()
  config_h.set('HAVE_UDISKS2', 1,
               description: 'Define if udisks2 is available')
endif
if goa_dep.found()
  config_h.set('HAVE_GOA', 1,
               description: 'Define if gnome-online-accounts is available')
endif
colord_gtk_dep = dependency('colord-gtk4', version: '>= 0.1.24', required: false)
if colord_gtk_dep.found()
  config_h.set('HAVE_COLORD_GTK', 1,
               description: 'Define if colord-gtk is available')
endif
gsound_dep = dependency('gsound', required: false)
if gsound_dep.found()
  config_h.set('HAVE_GSOUND', 1,
               description: 'Define if gsound is available')
endif

m_dep = cc.find_library('m')

common_deps = [
  gio_dep,
  glib_dep,
  gsettings_desktop_dep,
  libadwaita_dep,
  dependency('gio-unix-2.0'),
  dependency('gthread-2.0'),
  gtk_dep,
]

polkit_gobject_dep = dependency('polkit-gobject-1', version: '>= 0.103')
# Also verify that polkit ITS files exist:
# https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/491
polkit_files = [ 'gettext/its/polkit.its', 'gettext/its/polkit.loc' ]
foreach polkit_file: polkit_files
  r = run_command('build-aux/meson/find_xdg_file.py', polkit_file, check: true)
  assert(r.returncode() == 0, 'ITS support missing from polkit, please upgrade or contact your distribution')
endforeach

# Fil-C: CUPS is not part of the pizlonated system; the printers panel is
# dropped and the cups checks are skipped entirely.
cups_dep = dependency('cups', version : '>= 1.4', required: false)
if cups_dep.found()
  config_h.set('HAVE_CUPS', 1,
               description: 'Define if CUPS is available')

  # https://bugzilla.gnome.org/show_bug.cgi?id=696766
  cups_cflags = []
  if cups_dep.version().version_compare('>= 1.6')
    cups_cflags += '-D_PPD_DEPRECATED='
  endif

  # cups headers
  check_headers = [
    ['HAVE_CUPS_CUPS_H', 'cups/cups.h'],
    ['HAVE_CUPS_PPD_H', 'cups/ppd.h']
  ]

  foreach header: check_headers
    assert(cc.has_header(header[1], args: cups_cflags), 'CUPS headers not found: ' + header[1])
  endforeach

  config_h.set10('HAVE_CUPS_HTTPCONNECT2',
                 cc.has_function('httpConnect2', dependencies: cups_dep),
                 description: 'Define if httpConnect2() is available in CUPS')
endif

# IBus support
enable_ibus = get_option('ibus')
if enable_ibus
  ibus_dep = dependency('ibus-1.0', version: '>= 1.5.2')
endif
config_h.set('HAVE_IBUS', enable_ibus,
             description: 'Defined if IBus support is enabled')

# thunderbolt
config_h.set10('HAVE_FN_EXPLICIT_BZERO',
               cc.has_function('explicit_bzero', prefix: '''#include <string.h>'''),
               description: 'Define if explicit_bzero is available')

# Snap support
enable_snap = get_option('snap')
if enable_snap
  json_glib_dep = dependency('json-glib-1.0')
  libsoup_dep = dependency('libsoup-3.0')
endif
config_h.set('HAVE_SNAP', enable_snap,
             description: 'Define to 1 if Snap support is enabled')

# malcontent support
enable_malcontent = get_option('malcontent')
if enable_malcontent
  malcontent_dep = dependency('malcontent-0', version: '>= 0.7.0')
  config_h.set('HAVE_MALCONTENT_0_10', malcontent_dep.version().version_compare('>= 0.10.0'),
               description: 'Define to 1 if malcontent ≥ 0.10.0')
endif
config_h.set('HAVE_MALCONTENT', enable_malcontent,
             description: 'Define to 1 if malcontent support is enabled')

# location services support
location_services = get_option('location-services')
config_h.set('HAVE_LOCATION_SERVICES', location_services.enabled(),
             description: 'Whether location services is enabled')

if host_is_linux
  # Fil-C: NetworkManager and ModemManager are not part of the pizlonated
  # system; the network and wwan panels are dropped (see panels/meson.build)
  # and the BUILD_*/HAVE_* network config macros stay undefined so the
  # panel-loader entries vanish.  mm-glib is still probed (optionally) for
  # the applications panel.
  mm_dep = dependency('mm-glib', version: '>= 0.7', required: false)
  network_manager_deps = []
endif
# Fil-C: BUILD_NETWORK / HAVE_NETWORK_MANAGER / BUILD_WWAN / HAVE_WWAN are
# intentionally not defined.

if host_is_linux_not_s390
  # Fil-C: gnome-bluetooth, libwacom and thunderbolt support are not part
  # of the pizlonated system; the bluetooth and wacom panels are dropped
  # (see panels/meson.build) and BUILD_BLUETOOTH / HAVE_BLUETOOTH /
  # BUILD_WACOM / HAVE_WACOM / BUILD_THUNDERBOLT stay undefined.
  message('Bluetooth, Wacom and Thunderbolt panels will not be built (their dependencies are not part of the pizlonated system)')
endif
# Fil-C: the shell links an empty wacom_deps list.
wacom_deps = []

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

po_dir = join_paths(meson.project_source_root(), 'po')
its_dir = join_paths(meson.project_source_root(), 'gettext')

install_subdir(
  'gettext',
  install_dir: control_center_datadir
)

top_inc = include_directories('.')
shell_inc = include_directories('shell')

subdir('data/icons')
subdir('po')
subdir('panels')
subdir('shell')
subdir('search-provider')

if get_option('tests')
  subdir('tests')
endif

if get_option('documentation')
  subdir('man')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
)

configure_file(
  output: 'config.h',
  configuration: config_h
)

summary({
  'Documentation': get_option('documentation'),
  'Tests': get_option('tests'),
  'Optimized': control_center_optimized,
})

summary({
  'GNOME Bluetooth': host_is_linux_not_s390,
  'NetworkManager': host_is_linux,
  'Wacom': host_is_linux_not_s390,
}, section: 'Dependencies')

summary({
  'IBus': enable_ibus,
  'Snap': enable_snap,
  'Malcontent': enable_malcontent,
}, section: 'Optional Dependencies')
