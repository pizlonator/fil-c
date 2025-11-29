gsk_expand_content_md_files = [
  'paths.md',
]

if get_option('documentation')
  gsk4_toml = configure_file(
    input: 'gsk4.toml.in',
    output: 'gsk4.toml',
    configuration: toml_conf,
    install: true,
    install_dir: docs_dir / 'gsk4',
  )

  custom_target('gsk4-doc',
    input: [ gsk4_toml, gsk_gir[0] ],
    output: 'gsk4',
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
    depend_files: [ gsk_expand_content_md_files ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
  )
endif
