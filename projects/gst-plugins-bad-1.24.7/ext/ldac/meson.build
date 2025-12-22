ldac_sources = [
  'ldac-plugin.c',
  'gstldacenc.c',
]

ldac_dep = dependency('ldacBT-enc', required : get_option('ldac'))

if ldac_dep.found()
  gstldac = library('gstldac',
    ldac_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, ldac_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstldac]
endif
