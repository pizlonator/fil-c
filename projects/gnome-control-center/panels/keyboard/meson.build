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

pc_conf = configuration_data()
pc_conf.set('prefix', control_center_prefix)
pc_conf.set('datarootdir', control_center_datadir)
pc_conf.set('datadir', control_center_datadir)
pc_conf.set('PACKAGE', meson.project_name())
pc_conf.set('VERSION', meson.project_version())

pc = 'gnome-keybindings.pc'

configure_file(
  input: pc + '.in',
  output: pc,
  install: true,
  install_dir: join_paths(control_center_datadir, 'pkgconfig'),
  configuration: pc_conf
)

xml_files = [
  '00-multimedia.xml',
  '01-input-sources.xml',
  '01-launchers.xml',
  '01-system.xml',
  '50-accessibility.xml'
]

foreach file: xml_files
  i18n.merge_file(
    input: file + '.in',
    output: file,
    po_dir: po_dir,
    data_dirs: its_dir,
    install: true,
    install_dir: join_paths(control_center_pkgdatadir, 'keybindings')
  )
endforeach

sources = files(
  'cc-xkb-modifier-dialog.c',
  'cc-keyboard-shortcut-row.c',
  'cc-keyboard-shortcut-group.c',
  'cc-keyboard-shortcut-dialog.c',
  'cc-keyboard-panel.c',
  'cc-keyboard-item.c',
  'cc-keyboard-manager.c',
  'cc-keyboard-shortcut-editor.c',
  'keyboard-shortcuts.c',
  'cc-ibus-utils.c',
  'cc-input-chooser.c',
  'cc-input-row.c',
  'cc-input-source.c',
  'cc-input-source-ibus.c',
  'cc-input-list-box.c',
  'cc-input-source-xkb.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true
)

deps = common_deps + [
  gnome_desktop_dep,
  x11_dep
]

if enable_ibus
  deps += ibus_dep
endif

if tecla_is_subproject
  keyboard_previewer_exec = get_option('prefix') / get_option('bindir') / 'tecla'
else
  keyboard_previewer_exec = tecla_dep.get_variable('app')
endif

keyboard_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc],
  dependencies: deps,
  c_args: cflags + [
    '-DKEYBOARD_PREVIEWER_EXEC="@0@"'.format(keyboard_previewer_exec)
  ]
)
panels_libs += keyboard_panel_lib

subdir('icons')
