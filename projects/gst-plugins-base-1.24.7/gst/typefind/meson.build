typefindfunctions_extra_defs = []
typefindfunctions_extra_deps = []
if not get_option('gio-typefinder').disabled()
  typefindfunctions_extra_defs += ['-DHAVE_GIO']
  typefindfunctions_extra_deps += [gio_dep]
endif

typefindfunctions_sources = [
'gsttypefindfunctions.c',
'gsttypefindfunctionsdata.c',
'gsttypefindfunctionsplugin.c',
'gsttypefindfunctionsriff.c',
'gsttypefindfunctionsstartwith.c',
]

gsttypefind = library('gsttypefindfunctions',
  typefindfunctions_sources,
  c_args : gst_plugins_base_args + typefindfunctions_extra_defs,
  include_directories: [configinc, libsinc],
  dependencies : [pbutils_dep, gst_base_dep, typefindfunctions_extra_deps],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gsttypefind]
