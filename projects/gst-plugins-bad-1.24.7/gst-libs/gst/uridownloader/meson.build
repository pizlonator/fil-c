urid_sources = [
  'gstfragment.c',
  'gsturidownloader.c',
]
urid_headers = [
  'uridownloader-prelude.h',
  'gstfragment.h',
  'gsturidownloader.h',
  'gsturidownloader_debug.h',
]
install_headers(urid_headers, subdir : 'gstreamer-1.0/gst/uridownloader')

pkg_name = 'gstreamer-downloader-1.0'
gsturidownloader = library('gsturidownloader-' + api_version,
  urid_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_URI_DOWNLOADER', '-DG_LOG_DOMAIN="GStreamer-UriDownloader"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gstbase_dep],
)

gsturidownloader_dep = declare_dependency(link_with : gsturidownloader,
  include_directories : [libsinc],
  dependencies : [gstbase_dep])

gst_libraries += [[pkg_name, {'lib': gsturidownloader}]]
meson.override_dependency(pkg_name, gsturidownloader_dep)