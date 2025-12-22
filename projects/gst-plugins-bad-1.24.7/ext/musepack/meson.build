mpcdec_dep = cc.find_library('mpcdec', required: get_option('musepack'))

if mpcdec_dep.found() and cc.has_header_symbol('mpc/mpcdec.h', 'mpc_demux_init', dependencies: mpcdec_dep)
  gstmusepack = library('gstmusepack',
    'gstmusepackdec.c', 'gstmusepackreader.c',
    c_args : gst_plugins_bad_args,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstaudio_dep, mpcdec_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstmusepack]
elif get_option('musepack').enabled()
  error('Could not find libmpcdec header files, but Musepack was enabled via options')
endif
