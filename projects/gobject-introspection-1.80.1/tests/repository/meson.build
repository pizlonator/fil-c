if glib_dep.type_name() == 'pkgconfig'

  repository_tests = {
    'gitestrepo': typelibs,
    'giteststructinfo': typelibs,
    'gitestthrows': typelibs,
    'gitypelibtest': [regress_typelib],
  }

  repository_test_env = environment()
  repository_test_env.prepend(
    'GI_TYPELIB_PATH',
    join_paths(build_root, 'gir'),
    join_paths(build_root, 'tests'),
    join_paths(build_root, 'tests', 'scanner'),
  )
  repository_test_env.prepend(
    'LD_LIBRARY_PATH',
    join_paths(build_root, 'tests', 'scanner'),
  )
  if host_system == 'windows'
    repository_test_env.prepend(
      'PATH', join_paths(build_root, 'tests', 'scanner'))
  endif

  custom_c_args = cc.get_supported_arguments([
    '-Wno-unused-parameter',
  ])

  foreach test_name, depends : repository_tests
    source = files(test_name + '.c')
    exe = executable(test_name, source,
      dependencies: [girepo_dep, gio_dep],
      c_args: custom_c_args)
    test(test_name, exe, env: repository_test_env, depends: depends)
  endforeach
endif
