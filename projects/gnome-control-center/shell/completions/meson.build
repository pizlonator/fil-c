completion_conf = configuration_data()
completion_conf.set('PANELS', ' '.join(panels_list))

desktop = 'gnome-control-center'

desktop_in = configure_file(
  input: desktop + '.in',
  output: desktop,
  configuration: completion_conf,
  install: true,
  install_dir: join_paths(control_center_datadir, 'bash-completion', 'completions')
)
