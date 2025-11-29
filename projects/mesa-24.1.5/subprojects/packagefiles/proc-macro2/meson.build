project(
  'proc-macro2',
  'rust',
  version : '1.0.56',
  license :'MIT OR Apache-2.0',
)

rc = meson.get_compiler('rust')

if rc.version().version_compare('< 1.31')
  error('Minimum rustc supported version is 1.31')
endif

rust_args = [
  '--cfg', 'feature="proc-macro"',
  '--cfg', 'feature="default"',
  '--cfg', 'use_proc_macro',
  '--cfg', 'wrap_proc_macro',
]
if rc.version().version_compare('< 1.32')
  rust_args += ['--cfg', 'no_libprocmacro_unwind_safe']
endif
if rc.version().version_compare('< 1.39')
  rust_args += ['--cfg', 'no_bind_by_move_pattern_guard']
endif
if rc.version().version_compare('< 1.44')
  rust_args += ['--cfg', 'no_lexerror_display']
endif
if rc.version().version_compare('< 1.45')
  rust_args += ['--cfg', 'no_hygiene']
endif
if rc.version().version_compare('< 1.47')
  rust_args += ['--cfg', 'no_ident_new_raw']
endif
if rc.version().version_compare('< 1.54')
  rust_args += ['--cfg', 'no_literal_from_str']
endif
if rc.version().version_compare('< 1.55')
  rust_args += ['--cfg', 'no_group_open_close']
endif
if rc.version().version_compare('< 1.57')
  rust_args += ['--cfg', 'no_is_available']
endif
if rc.version().version_compare('< 1.66')
  rust_args += ['--cfg', 'no_source_text']
endif
if rc.version().version_compare('< 1.79')
  rust_args += [
    '--cfg', 'no_literal_byte_character',
    '--cfg', 'no_literal_c_string',
    ]
endif

u_ind = subproject('unicode-ident').get_variable('lib')

lib = static_library(
  'proc_macro2',
  'src/lib.rs',
  rust_args : rust_args,
  override_options : ['rust_std=2021', 'build.rust_std=2021'],
  link_with : u_ind,
  rust_abi : 'rust',
  native : true,
)
