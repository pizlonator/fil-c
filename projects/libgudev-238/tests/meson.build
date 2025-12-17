libgudev_tests = {
  'test-gudevdevice': {
    'dependencies': [gio_dep, libgudev_dep],
    'link_with': [libgudev],
    'sources': ['../gudev/gudevdevice.c'],
    'include_directories': [include_directories('../')],
  },
  'test-enumerator-filter': {
    'dependencies': [libgudev_dep, gio_dep, umockdev_dep],
    'environment': ['LD_PRELOAD=libumockdev-preload.so.0'],
  },
  'test-sysfsattr': {
    'dependencies': [libgudev_dep, gio_dep, umockdev_dep],
    'environment': ['LD_PRELOAD=libumockdev-preload.so.0'],
  },
  'test-double': {
    'dependencies': [libgudev_dep, gio_dep, umockdev_dep],
    'environment': ['LD_PRELOAD=libumockdev-preload.so.0', 'LC_NUMERIC=fr_FR.UTF-8'],
  },
}

foreach test_name, test_extras: libgudev_tests
  test_sources = ['@0@.c'.format(test_name)]

  test_sources += test_extras.get('sources', [])
  test_deps = test_extras.get('dependencies', [])
  test_env = test_extras.get('environment', [])
  test_include = test_extras.get('include_directories', [])

  test_exe = executable(
    test_name,
    test_sources,
    dependencies: test_deps,
    include_directories: test_include,
    c_args: ['-D_GUDEV_COMPILATION']
  )

  test(
    test_name,
    test_exe,
    env: test_env,
  )
endforeach
