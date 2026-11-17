

includes = [top_inc, include_directories('../../panels/network', 'nm-utils')]
cflags = [
  '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir()),
  '-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_WITH_GLIB',
  '-DNETWORKMANAGER_COMPILATION_TEST',
  '-DTEST_NM_SERVICE="@0@"'.format(join_paths(meson.project_source_root(), 'tests', 'network', 'nm-utils', 'test-networkmanager-service.py')),
]

envs = [
  'G_MESSAGES_DEBUG=all',
          'BUILDDIR=' + meson.current_build_dir(),
      'TOP_BUILDDIR=' + meson.project_build_root(),
# Disable ATK, this should not be required but it caused CI failures -- 2018-12-07
      'NO_AT_BRIDGE=1',
      'GTK_A11Y=none',
]

if Xvfb.found()
  exe = executable(
    'test-network-panel',
    ['test-network-panel.c', 'cc-test-window.c', 'nm-utils/nm-test-utils-impl.c'],
    include_directories : includes + [common_inc],
           dependencies : common_deps + network_manager_deps + [libtestshell_dep],
              link_with : [network_panel_lib],
                 c_args : cflags
  )

  test(
    'test-network-panel',
    find_program('test-network-panel.py'),
        env : envs,
    timeout : 60
  )
endif

exe = executable(
  'test-wifi-panel-text',
  ['test-wifi-text.c'],
  include_directories : includes + [common_inc],
  dependencies : common_deps + network_manager_deps + [libtestshell_dep],
  link_with : [network_panel_lib],
  c_args : cflags,
)

test(
  'test-wif-panel-text',
  exe,
  env : envs,
  timeout : 60
)
