sources = [
  'main.cpp',
  'gstthread.cpp',
  'pipeline.cpp',
  'qglrenderer.cpp',
]

moc_headers = [
  'gstthread.h',
  'pipeline.h',
  'qglrenderer.h',
]

qtwinsys_deps = []
if enabled_gl_winsys.contains('x11') and enabled_gl_platforms.contains('glx')
  qt5x11extras = dependency('qt5', modules : ['X11Extras'], required : false)
  if qt5x11extras.found()
    qtwinsys_deps += [qt5x11extras, glx_dep]
  else
    subdir_done()
  endif
endif

moc_files = qt5_mod.preprocess(moc_headers : moc_headers)
executable('qglwtextureshare', sources, moc_files,
  cpp_args : [gst_plugins_base_args] + qt_cxx_warn_less,
  include_directories: [configinc, libsinc],
  dependencies : [qt5core_dep, qt5gui_dep, qt5opengl_dep, qtwinsys_deps, gst_dep, video_dep, gstgl_dep, gstglproto_dep],
  install: false)
