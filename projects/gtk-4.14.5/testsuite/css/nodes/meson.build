nodetest_env = environment()
nodetest_env.set('GTK_A11Y', 'test')
nodetest_env.set('GSK_RENDERER', 'cairo')
nodetest_env.set('G_TEST_SRCDIR', meson.current_source_dir())
nodetest_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
nodetest_env.set('GSETTINGS_BACKEND', 'memory')
nodetest_env.set('G_ENABLE_DIAGNOSTIC', '0')

test_nodes = executable('test-css-nodes',
  sources: ['test-css-nodes.c', '../../testutils.c'],
  c_args: common_cflags,
  dependencies: libgtk_dep,
)

test('nodes', test_nodes,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  env: nodetest_env,
  suite: 'css',
)
