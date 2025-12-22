bz2_sources = [
  'gstbz2.c',
  'gstbz2dec.c',
  'gstbz2enc.c',
]

bz2_dep = cc.find_library('bz2', required : get_option('bz2'))

if bz2_dep.found() and cc.has_header_symbol('bzlib.h', 'BZ2_bzlibVersion')
  gstbz2 = library('gstbz2',
    bz2_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep, bz2_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstbz2]
endif
