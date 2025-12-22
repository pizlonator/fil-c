openal_dep = dependency('openal', method: 'pkg-config', version: '>= 1.14', required: get_option('openal'))

if openal_dep.found()
  gstopenal = library('gstopenal',
    'gstopenal.c', 'gstopenalelement.c', 'gstopenalsink.c', 'gstopenalsrc.c',
    c_args: gst_plugins_bad_args,
    include_directories: [configinc, libsinc],
    dependencies: [gstaudio_dep, openal_dep],
    install: true,
    install_dir: plugins_install_dir,
  )
  plugins += [gstopenal]
endif
