project(
  'quote',
  'rust',
  version : '1.0.25',
  license :'MIT OR Apache-2.0',
)

rc = meson.get_compiler('rust')

if rc.version().version_compare('< 1.31')
  error('Minimum rustc supported version is 1.31')
endif

rust_args = ['--cfg', 'feature="default"', '--cfg', 'feature="proc-macro"']
if rc.version().version_compare('< 1.53')
  rust_args += ['--cfg', 'needs_invalid_span_workaround']
endif

pm2 = subproject('proc-macro2').get_variable('lib')

# XXX: workround for meson bug: https://github.com/mesonbuild/meson/issues/11306
rust_args += ['-L', 'dependency=subprojects/unicode-ident']

lib = static_library(
  'quote',
  'src/lib.rs',
  rust_args : rust_args,
  override_options : ['rust_std=2018', 'build.rust_std=2018'],
  link_with : pm2,
  rust_abi : 'rust',
  native : true,
)
