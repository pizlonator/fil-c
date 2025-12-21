rtp_sources = files([
  'gstrtpbuffer.c',
  'gstrtcpbuffer.c',
  'gstrtppayloads.c',
  'gstrtphdrext.c',
  'gstrtpmeta.c',
  'gstrtpbaseaudiopayload.c',
  'gstrtpbasepayload.c',
  'gstrtpbasedepayload.c'
  ])

rtp_headers = files([
  'gstrtcpbuffer.h',
  'gstrtpbaseaudiopayload.h',
  'gstrtpbasedepayload.h',
  'gstrtpbasepayload.h',
  'gstrtpbuffer.h',
  'gstrtpdefs.h',
  'gstrtphdrext.h',
  'gstrtpmeta.h',
  'gstrtppayloads.h',
  'rtp-prelude.h',
  'rtp.h',
])
install_headers(rtp_headers, subdir : 'gstreamer-1.0/gst/rtp/')

rtp_enums = gnome.mkenums_simple('gstrtp-enumtypes',
  sources : rtp_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/rtp/rtp-prelude.h>',
  decorator : 'GST_RTP_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/rtp'))
gstrtp_enum_c = rtp_enums[0]
gstrtp_enum_h = rtp_enums[1]

gstrtp_deps = [audio_dep, gst_base_dep]
gst_rtp = library('gstrtp-@0@'.format(api_version),
  rtp_sources, gstrtp_enum_c, gstrtp_enum_h,
  c_args : gst_plugins_base_args + ['-DBUILDING_GST_RTP', '-DG_LOG_DOMAIN="GStreamer-RTP"'],
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : gstrtp_deps,
)

library_def = {'lib': gst_rtp}
pkg_name = 'gstreamer-rtp-1.0'
pkgconfig.generate(gst_rtp,
  libraries : [gst_dep, gst_base_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'RTP base classes and helper functions',
)

rtp_gen_sources = [gstrtp_enum_h]
if build_gir
  gst_gir_extra_args = gir_init_section + [ '--c-include=gst/rtp/rtp.h' ]
  gir = {
    'sources' : rtp_sources + rtp_headers + [gstrtp_enum_c] + [gstrtp_enum_h],
    'namespace' : 'GstRtp',
    'nsversion' : api_version,
    'identifier_prefix' : 'Gst',
    'symbol_prefix' : 'gst',
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0', 'GstBase-1.0'],
    'install' : true,
    'extra_args' : gst_gir_extra_args,
    'dependencies' : gstrtp_deps
  }
  library_def = {'lib': library_def['lib'], 'gir': [gir]}
  if not static_build
    rtp_gir = gnome.generate_gir(gst_rtp, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [rtp_gir]}
    rtp_gen_sources += [rtp_gir]
  endif
endif
gst_libraries += [[pkg_name, library_def]]

rtp_dep = declare_dependency(link_with : gst_rtp,
  include_directories : [libsinc],
  dependencies : gstrtp_deps,
  sources : rtp_gen_sources)

meson.override_dependency(pkg_name, rtp_dep)
