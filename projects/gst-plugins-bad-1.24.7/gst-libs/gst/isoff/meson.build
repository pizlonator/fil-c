isoff_sources = [
  'gstisoff.c',
]
isoff_headers = [
  'gstisoff.h',
]
install_headers(isoff_headers, subdir : 'gstreamer-1.0/gst/isoff')

gstisoff = library('gstisoff-' + api_version,
  isoff_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_ISOFF', '-DG_LOG_DOMAIN="GStreamer-ISOFF"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gstbase_dep],
)

gstisoff_dep = declare_dependency(link_with : gstisoff,
  include_directories : [libsinc],
  dependencies : [gstbase_dep])
