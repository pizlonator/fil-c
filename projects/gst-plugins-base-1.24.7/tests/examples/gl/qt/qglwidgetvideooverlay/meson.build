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

moc_files = qt5_mod.preprocess(moc_headers : moc_headers)
executable('qglwidgetvideoverlay', sources, moc_files,
  cpp_args : [gst_plugins_base_args] + qt_cxx_warn_less,
  include_directories: [configinc, libsinc],
  dependencies : [qt5core_dep, qt5gui_dep, qt5opengl_dep, gst_dep, video_dep, gstgl_dep, gstglproto_dep],
  install: false)
