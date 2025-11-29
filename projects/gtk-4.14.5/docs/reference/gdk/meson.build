expand_content_md_files = [
]

if get_option('documentation')
  gdk4_toml = configure_file(
    input: 'gdk4.toml.in',
    output: 'gdk4.toml',
    configuration: toml_conf,
    install: true,
    install_dir: docs_dir / 'gdk4',
  )

  custom_target('gdk4-doc',
    input: [ gdk4_toml, gdk_gir[0] ],
    output: 'gdk4',
    command: [
      gidocgen,
      'generate',
      gidocgen_common_args,
      '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gtk'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    depend_files: [ expand_content_md_files ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
  )

  if x11_enabled
    gdk4x11_toml = configure_file(
      input: 'gdk4-x11.toml.in',
      output: 'gdk4-x11.toml',
      configuration: toml_conf,
      install: true,
      install_dir: docs_dir / 'gdk4-x11',
    )

    custom_target('gdk4-x11-doc',
      input: [ gdk4x11_toml, gdk_x11_gir[0] ],
      output: 'gdk4-x11',
      command: [
        gidocgen,
        'generate',
        gidocgen_common_args,
        '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gtk'),
        '--config=@INPUT0@',
        '--output-dir=@OUTPUT@',
        '--content-dir=@0@'.format(meson.current_source_dir()),
        '@INPUT1@',
      ],
      depends: [ gdk_gir[0] ],
      depend_files: [ ],
      build_by_default: true,
      install: true,
      install_dir: docs_dir,
    )
  endif

  if wayland_enabled
    gdk4wayland_toml = configure_file(
      input: 'gdk4-wayland.toml.in',
      output: 'gdk4-wayland.toml',
      configuration: toml_conf,
      install: true,
      install_dir: docs_dir / 'gdk4-wayland',
    )

    custom_target('gdk4-wayland-doc',
      input: [ gdk4wayland_toml, gdk_wayland_gir[0] ],
      output: 'gdk4-wayland',
      command: [
        gidocgen,
        'generate',
        gidocgen_common_args,
        '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gtk'),
        '--config=@INPUT0@',
        '--output-dir=@OUTPUT@',
        '--content-dir=@0@'.format(meson.current_source_dir()),
        '@INPUT1@',
      ],
      depends: [ gdk_gir[0] ],
      depend_files: [ ],
      build_by_default: true,
      install: true,
      install_dir: docs_dir,
    )
  endif
endif
