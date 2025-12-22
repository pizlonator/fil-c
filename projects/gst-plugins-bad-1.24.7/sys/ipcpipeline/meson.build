ipcpipeline_sources = [
  'gstipcpipeline.c',
  'gstipcpipelineelement.c',
  'gstipcpipelinecomm.c',
  'gstipcpipelinesink.c',
  'gstipcpipelinesrc.c',
  'gstipcslavepipeline.c'
]

if get_option('ipcpipeline').disabled()
  subdir_done()
endif

gstipcpipeline = library('gstipcpipeline',
  ipcpipeline_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep] + winsock2,
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstipcpipeline]
