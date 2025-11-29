project(
  'syn',
  'rust',
  version : '2.0.15',
  license :'MIT OR Apache-2.0',
)

rc = meson.get_compiler('rust')


rust_args = [
  '--cfg', 'feature="default"',
  '--cfg', 'feature="derive"',
  '--cfg', 'feature="parsing"',
  '--cfg', 'feature="printing"',
  '--cfg', 'feature="clone-impls"',
  '--cfg', 'feature="proc-macro"',
  '--cfg', 'feature="quote"',
]
if rc.version().version_compare('< 1.36')
  rust_args += ['--cfg', 'syn_omit_await_from_token_macro']
endif
if rc.version().version_compare('< 1.39')
  rust_args += ['--cfg', 'syn_no_const_vec_new']
endif
if rc.version().version_compare('< 1.40')
  rust_args += ['--cfg', 'syn_no_non_exhaustive']
endif
if rc.version().version_compare('< 1.56')
  rust_args += ['--cfg', 'syn_no_negative_literal_parse']
endif

u_ind = subproject('unicode-ident').get_variable('lib')
quote = subproject('quote').get_variable('lib')
pm2 = subproject('proc-macro2').get_variable('lib')


lib = static_library(
  'syn',
  'src/lib.rs',
  rust_args : rust_args,
  override_options : ['rust_std=2021', 'build.rust_std=2021'],
  link_with : [u_ind, quote, pm2],
  rust_abi : 'rust',
  native : true,
)

dep_syn = declare_dependency(
  link_with : [lib, u_ind, quote, pm2],
)
