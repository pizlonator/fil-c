styletest_env = environment()
styletest_env.set('GTK_A11Y', 'test')
styletest_env.set('GSK_RENDERER', 'cairo')
styletest_env.set('G_TEST_SRCDIR', meson.current_source_dir())
styletest_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
styletest_env.set('GSETTINGS_BACKEND', 'memory')
styletest_env.set('G_ENABLE_DIAGNOSTIC', '0')

cssresources = gnome.compile_resources(
  'cssresources',
  'test-css-style.gresource.xml',
  source_dir: meson.current_source_dir(),
)

test_style = executable('test-css-style',
  sources: ['test-css-style.c', '../../testutils.c', cssresources],
  c_args: common_cflags,
  dependencies: libgtk_dep,
)

test('style', test_style,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  env: styletest_env,
  suite: 'css',
)
