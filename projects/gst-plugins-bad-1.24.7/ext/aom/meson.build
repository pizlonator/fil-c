aom_dep = dependency('aom', required: get_option('aom'))
aom3_dep = dependency('aom', version: '>= 3', required: get_option('aom'))
aom3_2_dep = dependency('aom', version: '>= 3.2', required: get_option('aom'))
if aom_dep.found()
  aom_defines = []
  if aom3_dep.found()
    aom_defines += ['-DHAVE_LIBAOM_3']
  endif
  if aom3_2_dep.found()
    aom_defines += ['-DHAVE_LIBAOM_3_2']
  endif

  gstaom = library('gstaom',
    ['gstaom.c', 'gstav1enc.c', 'gstav1dec.c', 'gstav1utils.c'],
    c_args : gst_plugins_bad_args + aom_defines,
    include_directories : [configinc],
    dependencies : [gstpbutils_dep, gstvideo_dep, aom_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstaom]
endif
