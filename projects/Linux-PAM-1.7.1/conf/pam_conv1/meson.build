pam_conv_y = custom_target(
  'pam_conv_y.[ch]',
  input: 'pam_conv_y.y',
  output: ['pam_conv_y.c', 'pam_conv_y.h'],
  command: yacc_cmd,
)

pam_conv_l = custom_target(
  'pam_conv_l.c',
  input: 'pam_conv_l.l',
  output: 'pam_conv_l.c',
  depends: pam_conv_y,
  command: [prog_flex, '-o', '@OUTPUT@', '@INPUT@'],
)

executable(
  'pam_conv1',
  sources: [pam_conv_l, pam_conv_y],
  include_directories: [libpam_inc],
  c_args: [
    '-Wno-unused-function',
    '-Wno-sign-compare',
  ],
  link_args: exe_link_args,
)
