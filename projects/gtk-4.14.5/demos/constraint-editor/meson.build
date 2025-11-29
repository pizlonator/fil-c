constraint_editor_sources = [
  'main.c',
  'constraint-editor-application.c',
  'constraint-editor-window.c',
  'constraint-view.c',
  'constraint-editor.c',
  'guide-editor.c',
]

constraint_editor_resources = gnome.compile_resources('constraint_editor_resources',
  'constraint-editor.gresource.xml',
  source_dir: meson.current_source_dir(),
)

executable('gtk4-constraint-editor',
  sources: [ constraint_editor_sources, constraint_editor_resources, ],
  c_args: common_cflags,
  dependencies: libgtk_dep,
  include_directories: confinc,
  win_subsystem: 'windows',
  link_args: extra_demo_ldflags,
  install: false,
)
