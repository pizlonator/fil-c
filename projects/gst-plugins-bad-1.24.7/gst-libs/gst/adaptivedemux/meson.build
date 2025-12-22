adaptivedemux_sources = files('gstadaptivedemux.c')
adaptivedemux_headers = files('gstadaptivedemux.h')

pkg_name = 'gstreamer-adaptivedemux-1.0'
gstadaptivedemux = library('gstadaptivedemux-' + api_version,
  adaptivedemux_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_ADAPTIVE_DEMUX', '-DG_LOG_DOMAIN="GStreamer-AdaptiveDemux"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gstbase_dep, gsturidownloader_dep],
)
gst_libraries += [[pkg_name, {'lib': gstadaptivedemux}]]

gstadaptivedemux_dep = declare_dependency(link_with : gstadaptivedemux,
  include_directories : [libsinc],
  dependencies : [gstbase_dep, gsturidownloader_dep])

meson.override_dependency(pkg_name, gstadaptivedemux_dep)