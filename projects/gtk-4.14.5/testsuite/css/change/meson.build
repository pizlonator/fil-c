changetest_env = environment()
changetest_env.set('GTK_A11Y', 'test')
changetest_env.set('GSK_RENDERER', 'cairo')
changetest_env.set('G_TEST_SRCDIR', meson.current_source_dir())
changetest_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
changetest_env.set('GSETTINGS_BACKEND', 'memory')
changetest_env.set('G_ENABLE_DIAGNOSTIC', '0')

test_change = executable('test-css-change',
  sources: ['test-css-change.c', '../../testutils.c'],
  c_args: common_cflags,
  dependencies: libgtk_dep,
)

test('change', test_change,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  env: changetest_env,
  suite: 'css',
)
