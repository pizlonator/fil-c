foreach program: proxy_test_programs
  test_name = program[0]
  if program[0] != program[1]
    test_name = program[0] + '-' + program[1]
  endif

  test_conf = configuration_data()
  test_conf.set('installed_tests_dir', installed_tests_execdir)
  test_conf.set('program', test_name)
  test_conf.set('environment', 'GIO_PROXY_TEST_NAME=' + program[1])

  if enable_installed_tests
    configure_file(
      input: test_template,
      output: test_name + '.test',
      install_dir: installed_tests_metadir,
      configuration: test_conf
    )
  endif

  c_flags = []

  if libproxy_dep.version() >= '0.5.0'
    c_flags += '-DLIBPROXY_0_5'
  endif

  exe = executable(
    test_name,
    program[0] + '.c',
    c_args : c_flags,
    include_directories: top_inc,
    dependencies: program[2],
    install: enable_installed_tests,
    install_dir: installed_tests_execdir
  )

  envs = [
    'G_TEST_SRCDIR=' + meson.current_source_dir(),
    'G_TEST_BUILDDIR=' + meson.current_build_dir(),
    'GIO_MODULE_DIR=' + join_paths(meson.project_build_root(), 'proxy', program[1]),
    'GIO_PROXY_TEST_NAME=' + program[1]
  ]

  test(
    test_name,
    exe,
    env: envs
  )
endforeach
