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

sources = files(
  'cc-camera-page.c',
  'cc-diagnostics-page.c',
  'cc-firmware-security-boot-dialog.c',
  'cc-firmware-security-dialog.c',
  'cc-firmware-security-help-dialog.c',
  'cc-firmware-security-page.c',
  'cc-firmware-security-utils.c',
  'cc-location-page.c',
  'cc-microphone-page.c',
  'cc-privacy-panel.c',
  'cc-screen-page.c',
  'cc-usage-page.c',
)

if host_is_linux_not_s390
  sources += files(
    'bolt-client.c',
    'bolt-device.c',
    'bolt-enums.c',
    'bolt-error.c',
    'bolt-names.c',
    'bolt-proxy.c',
    'bolt-str.c',
    'bolt-time.c',
    'cc-bolt-device-dialog.c',
    'cc-bolt-device-entry.c',
    'cc-bolt-page.c',
  )

  enum_headers = [
    'bolt-enums.h',
    'bolt-error.h'
  ]

  sources += gnome.mkenums_simple(
    'bolt-enum-types',
    sources: enum_headers,
  )
endif


sources += gnome.mkenums_simple(
  'cc-screen-page-enums',
  sources: ['cc-screen-page.h'],
)

sources += gnome.mkenums_simple(
  'cc-usage-page-enums',
  sources: ['cc-usage-page.h'],
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true
)

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir)
]

deps = common_deps + [
  dependency('libgtop-2.0'),
]

deps = deps + [
  polkit_gobject_dep,
  m_dep,
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
