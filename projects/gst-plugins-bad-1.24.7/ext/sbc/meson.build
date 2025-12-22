sbc_sources = [
  'sbc-plugin.c',
  'gstsbcdec.c',
  'gstsbcenc.c',
]

sbc_dep = dependency('sbc', version : '>= 1.0', required : get_option('sbc'))

if sbc_dep.found()
  gstsbc = library('gstsbc',
    sbc_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, sbc_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstsbc]
endif
