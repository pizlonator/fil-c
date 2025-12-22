libgstwebrtcnice_sources = files([
    'nice.c',
    'nicestream.c',
    'nicetransport.c',
])

libgstwebrtcnice_headers = files([
    'nice_fwd.h',
    'nice.h',
    'nicestream.h',
    'nicetransport.h',
])

libgstwebrtcnice_dep = dependency('', required : false)

libnice_dep = dependency('nice', version : '>=0.1.20', required : get_option('webrtc'),
                         fallback : ['libnice', 'libnice_dep'],
                         default_options: ['tests=disabled'])

deps = [gstwebrtc_dep, libnice_dep]

if libnice_dep.found()
  libnice_version = libnice_dep.version()
  libnice_c_args = []
  if libnice_version.version_compare('> 0.1.21.1')
    libnice_c_args += '-DHAVE_LIBNICE_CONSENT_FIX'
  endif
  libgstwebrtcnice = library('gstwebrtcnice-' + api_version,
    libgstwebrtcnice_sources, libgstwebrtcnice_headers,
    c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_WEBRTCNICE', '-DG_LOG_DOMAIN="GStreamer-webrtcnice"'] + libnice_c_args,
    include_directories: [configinc],
    version : libversion,
    soversion : soversion,
    darwin_versions : osxversion,
    dependencies: deps,
    install: true,
  )

  pkg_name = 'gstreamer-webrtc-nice-1.0'
  gst_libraries += [[pkg_name, {'lib': libgstwebrtcnice}]]
  pkgconfig.generate(libgstwebrtcnice,
    libraries : [deps],
    variables : pkgconfig_variables,
    subdirs : pkgconfig_subdirs,
    name : pkg_name,
    description : 'libnice based implementaion for GstWebRTCICE',
  )

  libgstwebrtcnice_dep = declare_dependency(link_with: libgstwebrtcnice,
    dependencies: [deps])

  install_headers(libgstwebrtcnice_headers, subdir : 'gstreamer-1.0/gst/webrtc/nice')
  meson.override_dependency(pkg_name, libgstwebrtcnice_dep)
endif
