sub_build_root = meson.project_build_root() / 'subprojects'
test_env_common_path = []
if glib_dep.type_name() == 'internal' and host_system == 'windows'
  test_env_common_path += [
    sub_build_root / 'glib/glib',
    sub_build_root / 'glib/gobject',
    sub_build_root / 'glib/gio',
    sub_build_root / 'glib/gmodule',
    sub_build_root / 'proxy-libintl',
    sub_build_root / 'zlib',
  ]
endif
if libffi_dep.type_name() == 'internal' and host_system == 'windows'
  test_env_common_path += [ sub_build_root / 'libffi/src' ]
endif

test_env_common_pypath = [ meson.project_build_root() ]


test_regress_sources = files('scanner/regress.c')
test_regress_headers = files('scanner/regress.h')
test_warnlib_sources = files('scanner/warnlib.c')
test_warnlib_headers = files('scanner/warnlib.h')
test_marshalling_sources = files('gimarshallingtests.c')
test_marshalling_headers = files('gimarshallingtests.h')

test_regress_incdirs = include_directories(['.', 'scanner'])

installed_sources = [
  'scanner/annotation.c',
  'scanner/annotation.h',
  'scanner/drawable.c',
  'scanner/drawable.h',
  'scanner/foo.c',
  'scanner/foo.h',
  'scanner/regress.c',
  'scanner/regress.h',
  'scanner/utility.c',
  'scanner/utility.h',
  'scanner/warnlib.c',
  'scanner/warnlib.h',
  'gimarshallingtests.c',
  'gimarshallingtests.h',
  'gitestmacros.h',
]

test_install_dir = get_option('datadir') / 'gobject-introspection-1.0/tests'

install_data(installed_sources, install_dir: test_install_dir)

# Will be overridden to newly built one in common-case regular, non-cross
# build.
test_girscanner = find_program('g-ir-scanner', native: true, required : false)
test_gircompiler = find_program('g-ir-compiler', native: false, required : false)

# Generate everything.h and everything.c:
test_everything_files = custom_target('everything',
  output: ['everything.h', 'everything.c'],
  depends: typelibs,
  env: g_ir_scanner_env,
  command: [
    test_girscanner,
    '--quiet',
    '--generate-typelib-tests=Everything,@OUTPUT0@,@OUTPUT1@',
    '--function-decoration=_GI_TEST_EXTERN',
    '--include-first-in-src=config.h',
    '--include-last-in-header=gitestmacros.h',
    '--include-last-in-header=sys/types.h',
    '--include-last-in-header=time.h',
  ],
  install: true,
  install_dir: test_install_dir,
)

test_everything_headers = [test_everything_files[0]]
test_everything_sources = [test_everything_files[1]]

custom_c_args = []
if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-unused-parameter',
  ])
endif

everything_lib = shared_library('everything-1.0',
  sources: test_everything_sources,
  include_directories : configinc,
  c_args: gi_hidden_visibility_cflags + custom_c_args,
  dependencies: [glib_dep, gobject_dep],
  version: '1.0.0',
)

if cc.get_id() != 'msvc'
  custom_c_args = cc.get_supported_arguments([
    '-Wno-unused-parameter',
  ])
endif

gimarshallingtests_lib = shared_library('gimarshallingtests-1.0',
  sources: test_marshalling_sources,
  include_directories : configinc,
  c_args: gi_hidden_visibility_cflags + custom_c_args,
  dependencies: [glib_dep, gobject_dep],
  version: '1.0.0',
)

test_gir_files = []
test_typelibs = []

# FIXME: When using glib as a subproject g-ir-scanner will not discover build
# flag of dependencies through pkg-config, instead they have to be specified
# manually like for example in gir/meson.build. This remain to be done here as
# well. Though, this could work differently altogether. Instead one could
# generate appropriate pkg-config files and point PKG_CONFIG_PATH to them,
# leaving all invocations of g-ir-scanner as is.
if (glib_dep.type_name() == 'pkgconfig' and
    test_girscanner.found() and
    get_option('build_introspection_data') == true)
  test_gir_files += [custom_target('gir-everything',
    input: test_everything_sources + test_everything_headers,
    output: 'Everything-1.0.gir',
    depends: [everything_lib] + typelibs,
    command: [
      test_girscanner,
      '--quiet',
      '--output=@OUTPUT@',
      '--no-libtool',
      '--reparse-validate',
      '--add-include-path=@0@'.format(source_root / 'gir'),
      '--add-include-path=@0@'.format(build_root / 'gir'),
      '--warn-all',
      '--warn-error',
      '--namespace=Everything',
      '--nsversion=1.0',
      '--include=Gio-2.0',
      '--library=everything-1.0',
      '-L', meson.current_build_dir(),
      '-I', meson.current_source_dir(),
      extra_giscanner_args,
      '@INPUT@',
    ]
  )]

  test_gir_files += [custom_target('gir-gimarshallingtests',
    input: test_marshalling_sources + test_marshalling_headers,
    output: 'GIMarshallingTests-1.0.gir',
    depends: [gimarshallingtests_lib] + typelibs,
    command: [
      test_girscanner,
      '--quiet',
      '--output=@OUTPUT@',
      '--no-libtool',
      '--reparse-validate',
      '--add-include-path=@0@'.format(source_root / 'gir'),
      '--add-include-path=@0@'.format(build_root / 'gir'),
      '--warn-all',
      '--warn-error',
      '--namespace=GIMarshallingTests',
      '--symbol-prefix=gi_marshalling_tests',
      '--nsversion=1.0',
      '--include=Gio-2.0',
      '--library=gimarshallingtests-1.0',
      '-L' + meson.current_build_dir(),
      '--c-include="tests/gimarshallingtests.h"',
      extra_giscanner_args,
      '@INPUT@',
    ]
  )]

  foreach gir : test_gir_files
    test_typelibs += custom_target('generate-typelib-@0@'.format(gir).underscorify(),
      input: gir,
      output: '@BASENAME@.typelib',
      depends: [gobject_gir, ],
      command: [
        test_gircompiler,
        '-o', '@OUTPUT@',
        '@INPUT@',
        '--includedir=@0@'.format(build_root / 'gir'),
        '--includedir=@0@'.format(meson.current_build_dir()),
      ],
    )
  endforeach

  # FIXME: Remove? Those tests doesn't actually do anything for typelibs ...
  foreach typelib : test_typelibs
    gi_tester = files('gi-tester')
    typelib_name = typelib.full_path().split('/')[-1]
    test(
      'gi-tester' + typelib_name,
      python,
      args: [gi_tester, typelib]
    )
  endforeach

endif

subdir('offsets')
if get_option('build_introspection_data') == true
  subdir('warn')
endif
subdir('scanner')
if get_option('build_introspection_data') == true
  subdir('repository')
endif
