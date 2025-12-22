sctp_sources = [
  'sctpreceivemeta.c',
  'sctpsendmeta.c',
]

sctp_headers = [
  'sctpreceivemeta.h',
  'sctpsendmeta.h',
  'sctp-prelude.h',
]

install_headers(sctp_headers, subdir : 'gstreamer-1.0/gst/sctp')

libgstsctp = library('gstsctp-' + api_version,
  sctp_sources,
  c_args : gst_plugins_bad_args + ['-DBUILDING_GST_SCTP', '-DG_LOG_DOMAIN="GStreamer-SCTP"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gstbase_dep],
)

pkg_name = 'gstreamer-sctp-1.0'
gst_libraries += [[pkg_name, {'lib': libgstsctp}]]
pkgconfig.generate(libgstsctp,
  libraries : [gst_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'SCTP helper functions',
)

gstsctp_dep = declare_dependency(link_with : libgstsctp,
  include_directories : [libsinc],
  dependencies : [gstbase_dep])

meson.override_dependency(pkg_name, gstsctp_dep)
