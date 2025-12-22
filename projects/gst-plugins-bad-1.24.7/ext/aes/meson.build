aes_sources = [
  'gstaes.c',
  'gstaeshelper.c',
  'gstaesenc.c',
  'gstaesdec.c',
]

aes_cargs = []
aes_dep = dependency('openssl', version : '>= 1.1.0', required : get_option('aes'))
if aes_dep.found()
  aes_cargs += ['-DHAVE_OPENSSL']
else
  subdir_done()
endif

gstaes = library('gstaes',
  aes_sources,
  c_args : gst_plugins_bad_args + aes_cargs,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstpbutils_dep, gstvideo_dep,
                  aes_dep, gio_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstaes]
aes_dep = declare_dependency(include_directories : include_directories('.'))
