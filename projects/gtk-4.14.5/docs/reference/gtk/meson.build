# XXX: gi-docgen needs a deps file
expand_content_md_files = [
  'overview.md',
  'broadway.md',
  'osx.md',
  'wayland.md',
  'windows.md',
  'x11.md',
  'getting_started.md',
  'initialization.md',
  'resources.md',
  'building.md',
  'compiling.md',
  'running.md',
  'migrating-2to4.md',
  'migrating-3to4.md',
  'actions.md',
  'input-handling.md',
  'drawing-model.md',
  'coordinates.md',
  'css-overview.md',
  'css-properties.md',
  'section-accessibility.md',
  'section-text-widget.md',
  'section-tree-widget.md',
  'section-list-widget.md',
  'question_index.md',
  'visual_index.md'
]

gtk_images = []

subdir('images')

if get_option('documentation')
  gtk4_toml = configure_file(
    input: 'gtk4.toml.in',
    output: 'gtk4.toml',
    configuration: toml_conf,
    install: true,
    install_dir: docs_dir / 'gtk4',
  )

  custom_target('gtk4-doc',
    input: [ gtk4_toml, gtk_gir[0] ],
    output: 'gtk4',
    command: [
      gidocgen,
      'generate',
      gidocgen_common_args,
      '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gtk'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--content-dir=@0@'.format(meson.current_build_dir()),
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    depends: [ gdk_gir[0], gsk_gir[0] ] + gtk_images,
    depend_files: [ expand_content_md_files ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
  )
endif

rst2man = find_program('rst2man', 'rst2man.py', required: false)
if get_option('man-pages') and not rst2man.found()
  error('No rst2man found, but man pages were explicitly enabled')
endif

if get_option('man-pages') and rst2man.found()
  rst_files = [
    [ 'gtk4-broadwayd', '1' ],
    [ 'gtk4-builder-tool', '1' ],
    [ 'gtk4-encode-symbolic-svg', '1', ],
    [ 'gtk4-launch', '1', ],
    [ 'gtk4-query-settings', '1', ],
    [ 'gtk4-rendernode-tool', '1' ],
    [ 'gtk4-update-icon-cache', '1', ],
    [ 'gtk4-path-tool', '1', ],
  ]

  if get_option('demos')
    rst_files += [
      [ 'gtk4-demo', '1', ],
      [ 'gtk4-demo-application', '1', ],
      [ 'gtk4-widget-factory', '1', ],
      [ 'gtk4-icon-browser', '1', ],
      [ 'gtk4-node-editor', '1', ],
    ]
  endif

  rst2man_flags = [
    '--syntax-highlight=none',
  ]

  foreach rst: rst_files
    man_name = rst[0]
    man_section = rst.get(1, '1')

    custom_target('man-@0@'.format(man_name),
      input: '@0@.rst'.format(man_name),
      output: '@0@.@1@'.format(man_name, man_section),
      command: [
        rst2man,
        rst2man_flags,
        '@INPUT@',
      ],
      capture: true,
      install: true,
      install_dir: get_option('mandir') / 'man@0@'.format(man_section),
    )
  endforeach
endif
