subparse_sources = [
  'gstssaparse.c',
  'gstsubparse.c',
  'gstsubparseelement.c',
  'gstsubparseplugin.c',
  'samiparse.c',
  'tmplayerparse.c',
  'mpl2parse.c',
  'qttextparse.c',
]

gstsubparse = library('gstsubparse',
  subparse_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [gst_base_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstsubparse]
