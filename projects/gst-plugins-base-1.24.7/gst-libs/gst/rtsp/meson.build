rtsp_sources = files([
  'gstrtsptransport.c',
  'gstrtspurl.c',
  'gstrtspmessage.c',
  'gstrtspconnection.c',
  'gstrtspdefs.c',
  'gstrtspextension.c',
  'gstrtsprange.c'
  ])

rtsp_headers = files([
  'gstrtspconnection.h',
  'gstrtspdefs.h',
  'gstrtspextension.h',
  'gstrtsp.h',
  'gstrtspmessage.h',
  'gstrtsprange.h',
  'gstrtsptransport.h',
  'gstrtspurl.h',
  'rtsp-prelude.h',
  'rtsp.h',
])
install_headers(rtsp_headers, subdir : 'gstreamer-1.0/gst/rtsp/')

rtsp_enums = gnome.mkenums_simple('gstrtsp-enumtypes',
  sources : rtsp_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/rtsp/rtsp-prelude.h>',
  decorator : 'GST_RTSP_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/rtsp'))
gstrtsp_c = rtsp_enums[0]
gstrtsp_h = rtsp_enums[1]

if host_system == 'windows'
  winsock2 = cc.find_library('ws2_32')
else
  winsock2 = []
endif
gstrtsp_deps = [gst_base_dep, gst_dep, gio_dep, libm, winsock2]

gst_rtsp = library('gstrtsp-@0@'.format(api_version),
  rtsp_sources,
  gstrtsp_h, gstrtsp_c,
  c_args : gst_plugins_base_args + ['-DBUILDING_GST_RTSP', '-DG_LOG_DOMAIN="GStreamer-RTSP"'],
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : gstrtsp_deps,
)

library_def = {'lib': gst_rtsp}
pkg_name = 'gstreamer-rtsp-1.0'
pkgconfig.generate(gst_rtsp,
  libraries : [gst_dep, sdp_dep, gio_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'RTSP base classes and helper functions',
)

rtsp_gen_sources = [gstrtsp_h]
if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/rtsp/rtsp.h' ]
  gir = {
    'sources' : rtsp_sources + rtsp_headers + [gstrtsp_c] + [gstrtsp_h],
    'namespace' : 'GstRtsp',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstBase-1.0', 'GstSdp-1.0', 'Gio-2.0'],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : gstrtsp_deps + [sdp_dep]
  }
  library_def = {'lib': library_def['lib'], 'gir': [gir]}
  if not static_build
    rtsp_gir = gnome.generate_gir(gst_rtsp, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [rtsp_gir]}
    rtsp_gen_sources += [rtsp_gir]
  endif
endif
gst_libraries += [[pkg_name, library_def]]

rtsp_dep = declare_dependency(link_with : gst_rtsp,
  include_directories : [libsinc],
  dependencies : gstrtsp_deps,
  sources : rtsp_gen_sources)

meson.override_dependency(pkg_name, rtsp_dep)
