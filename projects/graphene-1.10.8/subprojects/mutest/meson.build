project(
  'mutest', 'c',
  version: '2019.1',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=c99',
    'warning_level=2',
  ],
  license: 'MIT',
  meson_version: '>= 0.50.1',
)

add_project_arguments([ '-D_GNU_SOURCE' ], language: 'c')

mutest_version = meson.project_version().split('.')
mutest_major = mutest_version[0].to_int()
mutest_minor = mutest_version[1].to_int()

mutest_api_version = 1
mutest_soversion = '@0@.@1@.@2@'.format(0, mutest_major, mutest_minor)

mutest_prefix = get_option('prefix')
mutest_libdir = join_paths(mutest_prefix, get_option('libdir'))
mutest_datadir = join_paths(mutest_prefix, get_option('datadir'))
mutest_includedir = join_paths(mutest_prefix, get_option('includedir'))
mutest_libexecdir = join_paths(mutest_prefix, get_option('libexecdir'))

mutest_api_path = '@0@-@1@'.format(meson.project_name(), mutest_api_version)

cc = meson.get_compiler('c')
host_system = host_machine.system()

if cc.get_id() == 'msvc'
  test_cflags = [
    '-we4002',
    '-we4003',
    '-w14010',
    '-we4013',
    '-w14016',
    '-we4020',
    '-we4021',
    '-we4027',
    '-we4029',
    '-we4033',
    '-we4035',
    '-we4045',
    '-we4047',
    '-we4049',
    '-we4053',
    '-we4071',
    '-we4150',
    '-we4819'
  ]
else
  test_cflags = [
    '-fstrict-aliasing',
    '-Wcast-align',
    '-Wignored-qualifiers',
    '-Wimplicit-function-declaration',
    '-Wlogical-op',
    '-Wmissing-format-attribute',
    '-Wmissing-noreturn',
    '-Wmissing-include-dirs',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wpointer-arith',
    '-Wredundant-decls',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wswitch-enum',
    '-Wuninitialized',
    '-Wunused',
    '-Werror=address',
    '-Werror=array-bounds',
    '-Werror=empty-body',
    '-Werror=format=2',
    '-Werror=implicit',
    '-Werror=init-self',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=missing-declarations',
    '-Werror=missing-prototypes',
    '-Werror=nonnull',
    '-Werror=pointer-to-int-cast',
    '-Werror=return-type',
    '-Werror=sequence-point',
    '-Werror=trigraphs',
    '-Werror=undef',
    '-Werror=vla',
    '-Werror=write-strings',
  ]
endif

common_flags = cc.get_supported_arguments(test_cflags)

config_h = configuration_data()

# Detect and set symbol visibility
if get_option('default_library') != 'static'
  if host_system == 'windows'
    config_h.set('DLL_EXPORT', true)
    config_h.set('MUTEST_PUBLIC', '__declspec(dllexport) extern')
    if cc.get_id() != 'msvc'
      add_project_arguments(['-fvisibility=hidden'], language: 'c')
    endif
  else
    config_h.set('MUTEST_PUBLIC', '__attribute__((visibility("default"))) extern')
    add_project_arguments(['-fvisibility=hidden'], language: 'c')
  endif
endif

buildtype = get_option('buildtype')
if buildtype.startswith('debug')
  add_project_arguments([ '-DMUTEST_ENABLE_DEBUG' ], language: 'c')
endif

static = get_option('static')
installable = not (static and meson.is_subproject())

subdir('include')
subdir('src')
subdir('tests')
