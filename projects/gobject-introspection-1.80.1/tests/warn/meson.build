warn_tests = [
  'annotationparser.h',
  'callback-invalid-scope.h',
  'callback-missing-scope.h',
  'invalid-allow-none.h',
  'invalid-array.h',
  'invalid-closure.h',
  'invalid-constructor.h',
  'invalid-element-type.h',
  'invalid-method.h',
  'invalid-nullable.h',
  'invalid-optional.h',
  'invalid-option.h',
  'invalid-out.h',
  'invalid-return.h',
  'invalid-transfer.h',
  'missing-element-type.h',
  'return-gobject.h',
  'unknown-parameter.h',
  'unresolved-type.h',
]

warn_test_env = environment()
warn_test_env.set('UNINSTALLED_INTROSPECTION_SRCDIR', source_root)
warn_test_env.set('TOP_BUILDDIR', build_root)

# FIXME: Glib as a subproject (used on Windows mostly).
if glib_dep.type_name() == 'pkgconfig'
  foreach warn_fn : warn_tests
    test(
      'warn-' + warn_fn.split('.')[0], python,
      workdir: meson.current_source_dir(),
      env: warn_test_env,
      args: ['warningtester.py', warn_fn],
    )
  endforeach
endif
