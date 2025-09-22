cc = meson.get_compiler('c')
pkg = import('pkgconfig')
dir_liblastlog2 = include_directories('src')
lib_lastlog2_sources = '''
  src/lastlog2.h
  src/lastlog2P.h
  src/lastlog2.c
'''.split()  

liblastlog2_sym = 'src/liblastlog2.sym'
liblastlog2_sym_path = '@0@/@1@'.format(meson.current_source_dir(), liblastlog2_sym)

if build_liblastlog2
  lib_lastlog2 = both_libraries(
    'lastlog2',
    lib_lastlog2_sources,
    include_directories : [dir_include],
    link_args : ['-Wl,--version-script=@0@'.format(liblastlog2_sym_path)],
    link_depends : liblastlog2_sym,
    dependencies : [lib_sqlite3],
    install : build_liblastlog2,
    version : liblastlog2_version,
  )

  lastlog2_dep = declare_dependency(link_with: lib_lastlog2, include_directories: dir_liblastlog2)

  lastlog2_tests = [
    'dlopen',
    'pam_lastlog2_output',
    'remove_entry',
    'rename_user',
    'write_read_user',
    'y2038_ll2_read_all',
    'y2038_sqlite3_time',
  ]
  libdl = cc.find_library('dl')

  pkg.generate(
    lib_lastlog2,
    description : 'library to manage last login data',
    subdirs : 'lastlog2',
    version : pc_version
  )
  meson.override_dependency('lastlog2', lastlog2_dep)

  install_headers('src/lastlog2.h', subdir : 'liblastlog2')

  foreach lastlog2_test: lastlog2_tests
    test_name = 'test_lastlog2_' + lastlog2_test
    exe = executable(
      test_name,
      'src/tests/tst_' + lastlog2_test + '.c',
      include_directories : [dir_include],
      link_with : [lib_common],
      dependencies : [libdl, lastlog2_dep],
    )
    # the test-setup expects the helpers in the toplevel build-directory
    link = meson.project_build_root() / test_name
    run_command('ln', '-srf', exe.full_path(), link,
      check : true)
  endforeach

endif  
