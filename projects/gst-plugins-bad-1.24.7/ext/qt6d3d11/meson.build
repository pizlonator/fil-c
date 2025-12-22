gstqt6d3d11_sources = [
  'gstqml6d3d11sink.cpp',
  'gstqsg6d3d11node.cpp',
  'gstqt6d3d11videoitem.cpp',
  'plugin.cpp',
]

moc_headers = [
  'gstqsg6d3d11node.h',
  'gstqt6d3d11videoitem.h'
]

have_qt6d3d11 = false

qt6_option = get_option('qt6d3d11')
if qt6_option.disabled()
  subdir_done()
endif

if not gstd3d11_dep.found()
  if qt6_option.enabled()
    error('qt6 plugin was enabled explicitly, but required d3d11 dep was not found')
  else
    subdir_done()
  endif
endif

qt6_mod = import('qt6')
if not qt6_mod.has_tools()
  if qt6_option.enabled()
    error('qt6 plugin was enabled, but qt specific tools were not found')
  endif
  subdir_done()
endif

qt6qml_dep = dependency('qt6', modules : ['Core', 'Gui', 'Qml', 'Quick'],
                        required: qt6_option)
if not qt6qml_dep.found()
  subdir_done()
endif

have_qt6d3d11 = true
moc_files = qt6_mod.preprocess(moc_headers : moc_headers)
gstqt6d3d11 = library('gstqt6d3d11', gstqt6d3d11_sources, moc_files,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  cpp_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  include_directories: [configinc, libsinc],
  dependencies : [gst_dep, gstvideo_dep, gstd3d11_dep, qt6qml_dep],
  override_options : ['cpp_std=c++17'],
  install: true,
  install_dir : plugins_install_dir
)
plugins += [gstqt6d3d11]
