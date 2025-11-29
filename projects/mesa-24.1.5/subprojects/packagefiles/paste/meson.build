project(
  'paste',
  'rust',
  version : '1.0.14',
  license : '(MIT or Apache-2.0)',
)

add_languages('rust', required: true)
rust = import('rust')

lib = rust.proc_macro(
  'paste',
  files('src/lib.rs')
)

dep_paste = declare_dependency(
  link_with : [lib],
)