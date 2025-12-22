mpegts_sources = files(
  'mpegts.c',
  'gstmpegtssection.c',
  'gstmpegtsdescriptor.c',
  'gst-dvb-descriptor.c',
  'gst-dvb-section.c',
  'gst-atsc-section.c',
  'gst-scte-section.c',
  'gst-mpegtspesmetadatameta.c',
  'gst-metadata-descriptor.c'
)

mpegts_headers = files(
  'gstmpegtssection.h',
  'gst-atsc-section.h',
  'gst-dvb-section.h',
  'gst-scte-section.h',
  'gst-hdmv-section.h',
  'gstmpegtsdescriptor.h',
  'gst-atsc-descriptor.h',
  'gst-dvb-descriptor.h',
  'gst-isdb-descriptor.h',
  'gst-mpegtspesmetadatameta.h',
  'gst-metadata-descriptor.h',
  'mpegts-prelude.h',
  'mpegts.h',
)
install_headers(mpegts_headers, subdir : 'gstreamer-1.0/gst/mpegts')

mpegts_enums = gnome.mkenums_simple('gstmpegts-enumtypes',
  sources : mpegts_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/mpegts/mpegts-prelude.h>',
  decorator : 'GST_MPEGTS_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/mpegts'))

mpegtsenumtypes_h = mpegts_enums[1]

gen_sources = [mpegtsenumtypes_h]

gstmpegts = library('gstmpegts-' + api_version,
  mpegts_sources, mpegts_enums,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_MPEGTS', '-DG_LOG_DOMAIN="GStreamer-MpegTS"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gst_dep],
)

library_def = {'lib': gstmpegts}
pkg_name = 'gstreamer-mpegts-1.0'
pkgconfig.generate(gstmpegts,
  libraries : [gst_dep, gstbase_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'GStreamer MPEG-TS support',
)

if build_gir
  gir = {
    'sources' : mpegts_sources + mpegts_headers,
    'namespace' : 'GstMpegts',
    'nsversion' : api_version,
    'identifier_prefix' : 'GstMpegts',
    'symbol_prefix' : ['gst_mpegts', 'gst'],
    'export_packages' : pkg_name,
    'includes' : ['Gst-1.0'],
    'install' : true,
    'extra_args' : gir_init_section + ['-DGST_USE_UNSTABLE_API'] + ['--c-include=gst/mpegts/mpegts.h'],
    'dependencies' : [gst_dep]
  }
  library_def += {'gir': [gir]}
  if not static_build
    mpegts_gir = gnome.generate_gir(gstmpegts, kwargs: gir)
    library_def += {'gir_targets':  library_def.get('gir_targets', []) + [mpegts_gir]}
    gen_sources += mpegts_gir
  endif
endif
gst_libraries += [[pkg_name, library_def]]

gstmpegts_dep = declare_dependency(link_with : gstmpegts,
  include_directories : [libsinc],
  dependencies : [gst_dep],
  sources : gen_sources)
meson.override_dependency(pkg_name, gstmpegts_dep)
