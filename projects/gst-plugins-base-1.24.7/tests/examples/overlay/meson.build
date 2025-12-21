if get_option('qt5').disabled()
  subdir_done()
endif

if x11_dep.found()
  if gtk_x11_dep.found()
    executable('gtk-videooverlay', 'gtk-videooverlay.c',
      c_args : gst_plugins_base_args,
      include_directories: [configinc, libsinc],
      dependencies : [gst_dep, video_dep, libm, x11_dep, gtk_x11_dep],
      install: false)
  endif

  if have_cxx                                      # check for C++ support
    qt5_mod = import('qt5')
    qt5widgets_dep = dependency('qt5', modules : ['Core', 'Gui', 'Widgets'],
                                required: get_option('examples'))

    if qt5widgets_dep.found() and qt5_mod.has_tools(required: get_option('examples'))
      executable('qt-videooverlay', 'qt-videooverlay.cpp',
        cpp_args : gst_plugins_base_args,
        include_directories: [configinc, libsinc],
        dependencies : [gst_dep, video_dep, libm, x11_dep, qt5widgets_dep],
        install: false)

      # FIXME: this doesn't work properly yet, no moc_qtgv-videooverlay.cpp generated
      moc_files = qt5_mod.preprocess(moc_headers : 'qtgv-videooverlay.h')#moc_sources : 'qtgv-videooverlay.cpp')
      executable('qtgv-videooverlay', 'qtgv-videooverlay.cpp', moc_files,
        cpp_args : gst_plugins_base_args,
        include_directories: [configinc, libsinc],
        dependencies : [gst_dep, video_dep, libm, x11_dep, qt5widgets_dep],
        install: false)
    endif
  endif
endif

if host_system == 'windows'
  executable('win32-videooverlay', 'win32-videooverlay.c',
    c_args : gst_plugins_base_args,
    include_directories : [configinc, libsinc],
    dependencies : [gst_dep, video_dep],
    install: false,
  )

  executable('win32-videooverlay-playbin', 'win32-videooverlay-playbin.c',
    c_args : gst_plugins_base_args,
    include_directories : [configinc, libsinc],
    dependencies : [gst_dep, video_dep],
    install: false,
  )
endif
