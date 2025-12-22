sources = [
  'gstunixfd.c',
  'gstunixfdsink.c',
  'gstunixfdsrc.c',
]

gstallocators_dep = dependency('gstreamer-allocators-1.0')

gio_unix_dep = dependency('gio-unix-2.0', required: get_option('unixfd'))
if not gio_unix_dep.found()
  subdir_done()
endif

gstunixfd_plugin = library('gstunixfd',
  sources,
  c_args: gst_plugins_bad_args,
  include_directories: [configinc],
  dependencies : [gstbase_dep, gstallocators_dep, gio_dep, gio_unix_dep],
  install : true,
  install_dir : plugins_install_dir,
)

plugins += [gstunixfd_plugin]
