if get_option('qt5').disabled()
  subdir_done()
endif

qt5_mod = import('qt5')

qt5gui_dep = dependency('qt5', modules : ['Core', 'Gui', 'Widgets'], required : false)
qt5opengl_dep = dependency('qt5', modules : ['OpenGL'], required : false)
#FIXME; other platforms
libgl = cc.find_library ('GL', required : false)

qt_cxx_warn_less = cxx.get_supported_arguments(['-Wno-aggregate-return'])

if qt5gui_dep.found()
  subdir('videooverlay')
  if libgl.found()
    subdir('mousevideooverlay')
    if qt5opengl_dep.found()
      subdir('qglwidgetvideooverlay')
      subdir('qglwtextureshare')
    endif
  endif
endif
