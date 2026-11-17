appdata_conf = configuration_data()
appdata_conf.set('appid', application_id)

appdata = 'org.gnome.Settings.appdata.xml'

appdata_file = i18n.merge_file(
  input: configure_file(
    input: appdata + '.in',
    output: 'org.gnome.Settings.appdata.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.appdata.xml'.format(application_id),
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'metainfo')
)

# Validate Appdata
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test(
    'validate-appdata',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', appdata_file.full_path()],
    depends: appdata_file
  )
endif
