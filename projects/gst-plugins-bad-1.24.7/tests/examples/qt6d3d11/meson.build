if not have_qt6d3d11
  subdir_done()
endif

qt6_example_deps = dependency('qt6',
                              modules : ['Core', 'Gui', 'Widgets', 'Qml', 'Quick'],
                              required: get_option('examples'))

if not qt6_example_deps.found()
  subdir_done()
endif

subdir('qml6d3d11sink')
subdir('qml6d3d11sink-dyn-add')