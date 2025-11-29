# Available keys for each test:
#
#  - 'name': the test name; used for the test and to determine the base
#            source file for the test (MANDATORY)
#  - 'sources': (array): additional sources needed by the test
#  - 'c_args': (array): additional compiler arguments
#  - 'link_args': (array): additional linker arguments
#  - 'suites': (array): additional test suites
tests = [
  { 'name': 'accessible' },
  { 'name': 'box' },
  { 'name': 'button' },
  { 'name': 'checkbutton' },
  { 'name': 'colordialogbutton' },
  { 'name': 'custom' },
  { 'name': 'dialog' },
  { 'name': 'entry' },
  { 'name': 'expander' },
  { 'name': 'flowbox' },
  { 'name': 'general' },
  { 'name': 'image' },
  { 'name': 'listbox' },
  { 'name': 'levelbar' },
  { 'name': 'passwordentry' },
  { 'name': 'progressbar' },
  { 'name': 'scrollbar' },
  { 'name': 'searchentry' },
  { 'name': 'separator' },
  { 'name': 'spinbutton' },
  { 'name': 'stack' },
  { 'name': 'switch' },
  { 'name': 'togglebutton' },
  { 'name': 'value' },
  { 'name': 'window' },
]

internal_tests = [
  { 'name': 'inscription' },
  { 'name': 'label' },
  { 'name': 'text' },
  { 'name': 'textview' },
  { 'name': 'names' },
]

is_debug = get_option('buildtype').startswith('debug')

test_cargs = []

foreach flag: common_cflags
  if flag not in ['-Werror=missing-prototypes', '-Wmissing-prototypes',
                  '-Werror=missing-declarations', '-Wmissing-declarations']
    test_cargs += flag
  endif
endforeach

test_env = environment()
test_env.set('GTK_A11Y', 'test')
test_env.set('GSK_RENDERER', 'cairo')
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('GSETTINGS_BACKEND', 'memory')
test_env.set('G_ENABLE_DIAGNOSTIC', '0')

foreach t : tests
  test_name = t.get('name')
  test_srcs = ['@0@.c'.format(test_name)] + t.get('sources', [])
  test_extra_cargs = t.get('c_args', [])
  test_extra_ldflags = t.get('link_args', [])
  test_extra_suites = t.get('suites', [])
  test_timeout = 60

  test_exe = executable(test_name,
    sources: test_srcs,
    c_args: test_cargs + test_extra_cargs,
    link_args: test_extra_ldflags,
    dependencies: libgtk_dep,
  )

  if test_extra_suites.contains('slow')
    test_timeout = 90
  endif

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    timeout: test_timeout,
    env: test_env,
    suite: ['a11y'] + test_extra_suites,
  )
endforeach

foreach t : internal_tests
  test_name = t.get('name')
  test_srcs = ['@0@.c'.format(test_name)] + t.get('sources', [])
  test_extra_cargs = t.get('c_args', [])
  test_extra_ldflags = t.get('link_args', [])
  test_extra_suites = t.get('suites', [])
  test_timeout = 60

  test_exe = executable(test_name,
    sources: test_srcs,
    c_args: test_cargs + test_extra_cargs + ['-DGTK_COMPILATION'],
    link_args: test_extra_ldflags,
    dependencies: libgtk_static_dep,
  )

  if test_extra_suites.contains('slow')
    test_timeout = 90
  endif

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    timeout: test_timeout,
    env: test_env,
    suite: ['a11y'] + test_extra_suites,
  )
endforeach
