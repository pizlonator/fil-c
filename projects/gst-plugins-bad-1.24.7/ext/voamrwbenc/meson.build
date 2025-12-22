voamrwbenc_dep = dependency('vo-amrwbenc', version: '>= 0.1.0', required: get_option('voamrwbenc'))

if voamrwbenc_dep.found()
  gstvoamrwbenc = library('gstvoamrwbenc',
    'gstvoamrwb.c', 'gstvoamrwbenc.c',
    c_args: gst_plugins_bad_args,
    link_args: noseh_link_args,
    include_directories: [configinc],
    dependencies: [gstaudio_dep, gstpbutils_dep, gsttag_dep, voamrwbenc_dep],
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstvoamrwbenc]

  install_data('GstVoAmrwbEnc.prs', install_dir: presetdir)
  env = environment()
  env.prepend('GST_PRESET_PATH', meson.current_source_dir())
  meson.add_devenv(env)
endif
