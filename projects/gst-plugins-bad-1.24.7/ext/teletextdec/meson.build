zvbi_dep = dependency('zvbi-0.2', required: get_option('teletext'))

if zvbi_dep.found()
  gstteletext = library('gstteletext',
    'gstteletextdec.c',
    c_args: gst_plugins_bad_args,
    link_args: noseh_link_args,
    include_directories: [configinc],
    dependencies: [gstvideo_dep, zvbi_dep],
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstteletext]
endif
