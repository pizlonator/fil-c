test_offsets_sources = files('offsets.c')
test_offsets_headers = files('offsets.h')

test_offsets_inc = include_directories('.', '..')

test_offsets_lib = shared_library('offsets-1.0',
  sources: test_offsets_sources,
  include_directories : [configinc, test_offsets_inc],
  c_args: gi_hidden_visibility_cflags,
  dependencies: [glib_dep, gobject_dep],
  version: '1.0.0',
)

if glib_dep.type_name() == 'pkgconfig'

  test_offsets_gir = custom_target('gir-offsets',
    input: test_offsets_sources + test_offsets_headers,
    output: 'Offsets-1.0.gir',
    depends: [test_offsets_lib] + typelibs,
    command: [
      test_girscanner,
      '--quiet',
      '--output=@OUTPUT@',
      '--no-libtool',
      '--reparse-validate',
      '--add-include-path', join_paths(source_root, 'gir'),
      '--add-include-path', join_paths(build_root, 'gir'),
      '--warn-all',
      '--warn-error',
      '--namespace=Offsets',
      '--nsversion=1.0',
      '--include=GObject-2.0',
      '--library=offsets-1.0',
      '-L', meson.current_build_dir(),
      '-I', meson.current_source_dir(),
      '-I', join_paths(source_root, 'tests'),
      extra_giscanner_args,
      '@INPUT@',
    ]
  )

  test_offsets_typelib = custom_target(
    'generate-typelib-offsets',
    input: test_offsets_gir,
    output: '@BASENAME@.typelib',
    depends: [gobject_gir, ],
    command: [test_gircompiler, '-o', '@OUTPUT@', '@INPUT@',
              '--includedir', join_paths(build_root, 'gir'),
              '--includedir', meson.current_build_dir()
    ],
  )

  gitestoffsets_exe = executable('gitestoffsets',
    'gitestoffsets.c', 
    include_directories : test_offsets_inc,
    dependencies: [girepo_dep],
  )

  test_offsets_env = environment()
  test_offsets_env.set('top_builddir', join_paths(build_root, 'gir'))
  test_offsets_env.set('builddir', meson.current_build_dir())
  if host_system == 'windows'
    test_offsets_env.prepend('PATH', join_paths(build_root, 'girepository'))
  endif

  test('test_offsets.py',
    python, 
    args: files('test_offsets.py'),
    env: test_offsets_env,
    depends: [gitestoffsets_exe, test_offsets_typelib],
  )

endif

