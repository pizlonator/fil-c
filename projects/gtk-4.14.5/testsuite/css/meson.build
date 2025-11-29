csstest_env = environment()
csstest_env.set('GTK_A11Y', 'test')
csstest_env.set('GSK_RENDERER', 'cairo')
csstest_env.set('G_TEST_SRCDIR', meson.current_source_dir())
csstest_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
csstest_env.set('GSETTINGS_BACKEND', 'memory')
csstest_env.set('G_ENABLE_DIAGNOSTIC', '0')
csstest_env.set('GDK_DEBUG', 'default-settings')

subdir('parser')
subdir('nodes')
subdir('style')
subdir('change')

test_api = executable('api',
  sources: ['api.c'],
  c_args: common_cflags,
  dependencies: libgtk_dep,
)

test('api', test_api,
  args: ['--tap', '-k' ],
  protocol: 'tap',
  env: csstest_env,
  suite: 'css',
)

test_data = executable('data',
  sources: ['data.c'],
  c_args: common_cflags + ['-DGTK_COMPILATION'],
  include_directories: [confinc, ],
  dependencies: libgtk_static_dep,
)

test('data', test_data,
  args: ['--tap', '-k' ],
  protocol: 'tap',
  env: csstest_env,
  suite: 'css',
)

transition = executable('transition',
  sources: ['transition.c'],
  c_args: common_cflags + ['-DGTK_COMPILATION'],
  dependencies: libgtk_static_dep,
)

test('transition', transition,
     args: [ '--tap', '-k' ],
     protocol: 'tap',
     env: csstest_env,
     suite: 'css'
)

if false and get_option ('profiler')

  adwaita_env = csstest_env
  adwaita_env.set('GTK_THEME', 'Adwaita')
  test('performance-adwaita', test_performance,
    args: [
      '--mark', 'css validation',
      '--name', 'performance-adwaita',
      '--output', join_paths(meson.current_build_dir(), 'output'),
      join_paths(meson.current_build_dir(), '../../demos/widget-factory/gtk4-widget-factory'),
    ],
    env: adwaita_env,
    suite: [ 'css' ],
  )

  empty_env = csstest_env
  empty_env.set('GTK_THEME', 'Empty')
  test('performance-empty', test_performance,
    args: [
      '--mark', 'css validation',
      '--name', 'performance-empty',
      '--output', join_paths(meson.current_build_dir(), 'output'),
      join_paths(meson.current_build_dir(), '../../demos/widget-factory/gtk4-widget-factory'),
    ],
    env: empty_env,
    suite: [ 'css' ],
  )
endif
