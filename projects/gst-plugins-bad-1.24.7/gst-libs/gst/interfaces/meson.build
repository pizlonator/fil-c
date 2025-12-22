photography_sources = ['photography.c']
photo_headers = ['photography.h', 'photography-prelude.h']
install_headers(photo_headers, subdir : 'gstreamer-1.0/gst/interfaces')

photo_enums = gnome.mkenums_simple('photography-enumtypes',
  sources : photo_headers,
  body_prefix : '#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif',
  header_prefix : '#include <gst/interfaces/photography-prelude.h>',
  decorator: 'GST_PHOTOGRAPHY_API',
  install_header: true,
  install_dir : join_paths(get_option('includedir'), 'gstreamer-1.0/gst/interfaces'))

photoenum_c = photo_enums[0]
photoenum_h = photo_enums[1]

pkg_name = 'gstreamer-photography-1.0'
gstphotography = library('gstphotography-' + api_version,
  photography_sources, photoenum_h, photoenum_c,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_PHOTOGRAPHY', '-DG_LOG_DOMAIN="GStreamer-Photography"'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gst_dep],
)

pkgconfig.generate(gstphotography,
  libraries : [gst_dep, gstbase_dep],
  variables : pkgconfig_variables,
  subdirs : pkgconfig_subdirs,
  name : pkg_name,
  description : 'High level API for transcoding using GStreamer',
)

gstphotography_dep = declare_dependency(link_with : gstphotography,
  include_directories : [libsinc],
  dependencies : [gst_dep],
  sources : [photoenum_h])

gst_libraries += [[pkg_name, {'lib': gstphotography}]]
meson.override_dependency(pkg_name, gstphotography_dep)
