env = environment()
env.prepend('GI_TYPELIB_PATH',
  project_build_root / 'gtk',
)
env.prepend('LD_PRELOAD', project_build_root / 'gtk' / 'libgtk-4.so')
env.prepend('MESON_CURRENT_SOURCE_DIR', meson.current_source_dir())
env.prepend('MESON_CURRENT_BUILD_DIR', meson.current_build_dir())

# asan does not work with our use of LD_PRELOAD for these tests
if get_option('b_sanitize') != 'address'
  test('monitor',
    find_program('run-headless-monitor-tests.sh', dirs: meson.current_source_dir()),
    suite: ['headless'],
    env: env,
  )

  test('input',
    find_program('run-headless-input-tests.sh', dirs: meson.current_source_dir()),
    suite: ['headless'],
    env: env,
  )
endif
