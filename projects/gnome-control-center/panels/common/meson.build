common_inc = include_directories('.')

common_sources = []

enums = 'gsd-common-enums'
enums_header = files('gsd-device-manager.h')

common_sources += gnome.mkenums(
  enums + '.h',
  sources: enums_header,
  fhead: '#pragma once\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GSD_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n'
)

common_sources += gnome.mkenums(
  enums + '.c',
  sources: enums_header,
  fhead: '#include "gsd-device-manager.h"\n#include "gsd-common-enums.h"\n',
  fprod: '\n/* enumerations from "@filename@" */',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {',
  vprod: '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static ("@EnumName@", values);\n  }\n  return etype;\n}\n'
)

common_sources += gnome.compile_resources(
  'cc-common-resources',
  'common.gresource.xml',
  c_name: 'cc_common',
  export: true
)

generates_sources_dep = declare_dependency(
  sources: common_sources,
)

sources = files(
  'cc-hostname-entry.c',
  'cc-time-entry.c',
  'hostname-helper.c',
)

libwidgets = static_library(
  'widgets',
  sources: sources,
  include_directories: top_inc,
  dependencies: common_deps + [ generates_sources_dep, polkit_gobject_dep ]
)
libwidgets_dep = declare_dependency(
  include_directories: common_inc,
  link_with: libwidgets
)

sources = common_sources + files(
  'cc-common-language.c',
  'cc-hostname.c',
  'cc-illustrated-row.c',
  'cc-language-chooser.c',
  'cc-language-row.c',
  'cc-list-row.c',
  'cc-list-row-info-button.c',
  'cc-time-editor.c',
  'cc-permission-infobar.c',
  'cc-split-row.c',
  'cc-vertical-row.c',
  'cc-util.c'
)

deps = common_deps + [
  generates_sources_dep,
  gnome_desktop_dep,
  dependency('fontconfig')
]

liblanguage = static_library(
  'language',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps
)

liblanguage_dep = declare_dependency(
  include_directories: common_inc,
  link_with: liblanguage
)

gsd_headers = [
  'gsd-device-manager.h',
  'gsd-input-helper.h'
]

gsd_sources = [
  'gsd-device-manager.c',
  'gsd-input-helper.c'
]

sources = common_sources + files(gsd_sources)

deps = common_deps + [ gudev_dep ]

libdevice = static_library(
  'device',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps
)

libdevice_dep = declare_dependency(
  include_directories: common_inc,
  link_with: libdevice
)

polkit_conf = configuration_data()
polkit_conf.set('PRIVILEGED_GROUP', get_option('privileged_group'))
configure_file(
  input: 'gnome-control-center.rules.in',
  output: 'gnome-control-center.rules',
  configuration: polkit_conf,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'rules.d')
)
