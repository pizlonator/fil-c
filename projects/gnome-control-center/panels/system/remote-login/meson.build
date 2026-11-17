polkit_conf = configuration_data()
polkit_conf.set('libexecdir', control_center_libexecdir)
polkit = 'org.gnome.controlcenter.remote-login-helper.policy'

polkit_in = configure_file(
  input: polkit + '.in.in',
  output: polkit + '.in',
  configuration: polkit_conf
) 
   
i18n.merge_file(
  input: polkit_in,
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
) 
