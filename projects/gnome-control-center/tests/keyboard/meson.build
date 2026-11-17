if setxkbmap.found() and Xvfb.found()
  test_units = [
    'test-keyboard-shortcuts'
  ]

  env = [
    'G_MESSAGES_DEBUG=all',
            'BUILDDIR=' + meson.current_build_dir(),
        'TOP_BUILDDIR=' + meson.project_build_root(),
  # Disable ATK, this should not be required but it caused CI failures -- 2018-12-07
        'NO_AT_BRIDGE=1'
  ]
  cflags = [
    '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir())
  ]
  includes = [top_inc, include_directories('../../panels/keyboard')]

  foreach unit: test_units
    exe = executable(
                      unit,
             [unit + '.c'],
             dependencies : common_deps,
      include_directories : includes,
                link_with : [keyboard_panel_lib],
                   c_args : cflags
    )
  endforeach

  test(
    'test-keyboard',
    find_program('test-keyboard.py'),
        env : env,
    timeout : 60
  )
endif
