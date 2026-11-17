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

cflags += [
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
]

sources = files(
  'cc-online-account-provider-row.c',
  'cc-online-account-row.c',
  'cc-online-accounts-panel.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify(),
  export: true
)

deps = common_deps + [
  goa_dep,
  dependency('goa-backend-1.0', version: goa_req_version),
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
