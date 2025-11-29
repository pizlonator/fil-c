bash = find_program('bash', required : false)
if bash.found()
  test_env = environment()

  foreach t : ['simplify', 'simplify-3to4', 'validate', 'enumerate', 'settings']
    test(t,
      find_program(t, dirs: meson.current_source_dir()),
      workdir: meson.current_build_dir(),
      protocol: 'tap',
      env: [
        'TEST_RESULT_DIR=@0@'.format(join_paths(meson.current_build_dir(), 'output')),
        'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
        'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
        'GTK_A11Y=test',
        'GTK_BUILDER_TOOL=@0@'.format(get_variable('gtk4_builder_tool').full_path()),
        'GTK_QUERY_SETTINGS=@0@'.format(get_variable('gtk4_query_settings').full_path())
      ],
      suite: 'tools',
    )
  endforeach
endif
