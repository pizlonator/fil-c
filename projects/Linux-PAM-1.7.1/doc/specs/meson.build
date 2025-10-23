parse_y = custom_target(
  'parse_y.[ch]',
  input: 'parse_y.y',
  output: ['parse_y.c', 'parse_y.h'],
  command: yacc_cmd,
)

parse_l = custom_target(
  'parse_l.c',
  input: 'parse_l.l',
  output: 'parse_l.c',
  depends: parse_y,
  command: [prog_flex, '-o', '@OUTPUT@', '@INPUT@'],
)

padout = executable(
  'padout',
  native: true,
  sources: [parse_l, parse_y],
  include_directories: [libpam_inc],
  c_args: [
    '-Wno-unused-function',
    '-Wno-sign-compare',
  ],
  link_args: exe_link_args,
)

custom_target(
  'draft-morgan-pam-current.txt',
  input: 'draft-morgan-pam.raw',
  output: 'draft-morgan-pam-current.txt',
  command: [redir_exe, '@INPUT@', '@OUTPUT@', padout],
  depends: padout,
  install: true,
  install_dir: docdir,
  install_tag: 'doc',
)

install_data(
  'rfc86.0.txt',
  install_dir: docdir,
  install_tag: 'doc',
)
