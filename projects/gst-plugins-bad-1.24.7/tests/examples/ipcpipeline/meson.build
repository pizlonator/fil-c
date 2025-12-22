if get_option('ipcpipeline').disabled()
  subdir_done()
endif

build_ipcpipeline_example = cc.has_header('sys/socket.h')
foreach f : ['kill', 'socketpair', 'fcntl', 'fork']
  if build_ipcpipeline_example
    build_ipcpipeline_example = cc.has_function(f)
  endif
endforeach

if not build_ipcpipeline_example
  subdir_done()
endif

executable('ipcpipeline1', 'ipcpipeline1.c',
  include_directories: [configinc],
  dependencies: [gst_dep, gstbase_dep],
  c_args: gst_plugins_bad_args,
  install: false)

executable('ipc-play', 'ipc-play.c',
  include_directories: [configinc],
  dependencies: [gst_dep, gstbase_dep, gstvideo_dep],
  c_args: gst_plugins_bad_args,
  install: false)
