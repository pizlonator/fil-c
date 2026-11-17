
test_units = [
  'test-timezone',
  'test-timezone-gfx',
  'test-endianess',
]

env = [
  'G_MESSAGES_DEBUG=all',
          'BUILDDIR=' + meson.current_build_dir(),
      'TOP_BUILDDIR=' + meson.build_root(),
# Disable ATK, this should not be required but it caused CI failures -- 2018-12-07
      'NO_AT_BRIDGE=1'
]
cflags = [
  '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir()),
  '-DSRCDIR="@0@"'.format(meson.project_source_root() + '/panels/datetime')
]

foreach unit: test_units
  exe = executable(
                    unit,
           [unit + '.c'],
           dependencies : common_deps + [m_dep, datetime_panel_lib_dep],
                 c_args : cflags
  )
endforeach

test(
  'test-datetime',
  find_program('test-datetime.py'),
      env : env,
  timeout : 60
)
