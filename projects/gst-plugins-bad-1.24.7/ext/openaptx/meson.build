openaptx_sources = [
  'openaptx-plugin.c',
  'gstopenaptxdec.c',
  'gstopenaptxenc.c',
]

if not get_option('openaptx').allowed()
  subdir_done()
endif

openaptx_defines = []

openaptx_dep = dependency('libfreeaptx', version : '>= 0.1.1', required : false)
if openaptx_dep.found()
  openaptx_defines += ['-DUSE_FREEAPTX']
else
  openaptx_dep = dependency('libopenaptx', version : '== 0.2.0', required : get_option('openaptx'))
endif

if openaptx_dep.found()
  gstopenaptx = library('gstopenaptx',
    openaptx_sources,
    c_args : gst_plugins_bad_args + openaptx_defines,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, openaptx_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstopenaptx]
endif
