custom_man_xsl = custom_target(
  'custom-man.xsl',
  input: 'custom-man.xsl.in',
  output: ['custom-man.xsl'],
  command: [
    redir_exe,
    '@INPUT@',
    '@OUTPUT@',
    'sed',
    's+MAN_STYLESHEET+' + man_stylesheet + '+g'
  ]
)

install_data(
  'index.html',
  install_dir: htmldir,
  install_tag: 'doc',
)

install_html = files('install-html.sh')

subdir('man')
subdir('specs')
subdir('sag')
subdir('adg')
subdir('mwg')
