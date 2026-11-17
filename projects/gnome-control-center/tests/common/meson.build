

test_units = [
  'test-hostname',
  # 'test-time-entry', # FIXME
]

cflags = [
  '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir()),
  '-DTEST_TOPSRCDIR="@0@"'.format(meson.project_source_root())
]

foreach unit: test_units
  exe = executable(
                  unit,
           unit + '.c',
    include_directories : [ top_inc, common_inc ],
           dependencies : common_deps + [libwidgets_dep],
                 c_args : cflags,
  )
  test(unit, exe)
endforeach
