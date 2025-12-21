tcp_sources = [
  'gstmultihandlesink.c',
  'gstmultisocketsink.c',
  'gstsocketsrc.c',
  'gsttcpclientsrc.c',
  'gsttcpclientsink.c',
  'gsttcpelements.c',
  'gsttcpserversrc.c',
  'gsttcpserversink.c',
  'gsttcpsrcstats.c',
  'gsttcpplugin.c',
]

if core_conf.has('HAVE_SYS_SOCKET_H')
   tcp_sources += 'gstmultifdsink.c'
endif

gsttcp = library('gsttcp',
  tcp_sources,
  c_args : gst_plugins_base_args,
  include_directories: [configinc, libsinc],
  dependencies : [gst_base_dep, gst_net_dep, gio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gsttcp]
